#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <unistd.h>
#include <algo/algo.h>
#include <netinet/in.h>
#include <sstream>

#include "torrent/exceptions.h"
#include "download/download_state.h"
#include "download/download_net.h"

#include "peer_connection.h"

#define BUFFER_SIZE (1<<9)

using namespace algo;

// This file is for functions that does not directly relate to the
// protocol logic. Or the split might be completely arbitary...

namespace torrent {

extern std::list<std::string> caughtExceptions;

PeerConnection::PeerConnection() :
  m_shutdown(false),
  m_stallCount(0),

  m_download(NULL),
  m_net(NULL),

  m_sendChoked(false),
  m_sendInterested(false),
  m_tryRequest(true),
  
  m_taskKeepAlive(sigc::mem_fun(*this, &PeerConnection::task_keep_alive)),
  m_taskSendChoke(sigc::mem_fun(*this, &PeerConnection::task_send_choke)),
  m_taskStall(sigc::mem_fun(*this, &PeerConnection::task_stall))
{
}

PeerConnection::~PeerConnection() {
  if (m_download) {
    if (m_requests.is_downloading())
      m_requests.skip();

    m_requests.cancel();

    if (m_down.state != READ_BITFIELD)
      m_download->get_bitfield_counter().dec(m_bitfield.get_bitfield());
  }

  delete [] m_up.buf;
  delete [] m_down.buf;

  if (m_fd >= 0)
    close(m_fd);
}

bool PeerConnection::writeChunk(int maxBytes) {
  StorageChunk::Node& part = m_up.data->get_position(m_sends.front().get_offset() + m_up.pos);

  unsigned int length = std::min(m_sends.front().get_length(),
				 part.position + part.length - m_sends.front().get_offset());

  // TODO: Make this a while loop so we spit out as much of the piece as we can this work cycle.

  write_buf(part.chunk.begin() + m_sends.front().get_offset() + m_up.pos - part.position,
	   std::min(length, m_up.pos + maxBytes),
	   m_up.pos);

  return m_up.pos == m_sends.front().get_length();
}

bool PeerConnection::readChunk() {
  if (m_down.pos > (1 << 17) + 9)
    throw internal_error("Really bad read position for buffer");
  
  const Piece& p = m_requests.get_piece();

  StorageChunk::Node& part = m_down.data->get_position(p.get_offset() + m_down.pos);

  unsigned int offset = p.get_offset() + m_down.pos - part.position;
  
  if (!part.chunk.is_valid() ||
      !part.chunk.is_write())
    throw storage_error("Tried to write piece to file area that isn't valid or can't be written to");
  
  if (!read_buf(part.chunk.begin() + offset,
	       std::min(part.position + part.chunk.size() - p.get_offset(), p.get_length()),
	       m_down.pos))
    return false; // Did not read the whole part of the piece
  
  if (m_down.pos != p.get_length())
    return false; // We still got parts of the piece to read

  return true;
}
  
void PeerConnection::bufCmd(Protocol cmd, unsigned int length, unsigned int send) {
  m_up.pos = m_up.length;
  m_up.length += (send ? send : length) + 4;

  if (m_up.length > BUFFER_SIZE)
    throw internal_error("PeerConnection write buffer to small");

  *(uint32_t*)(m_up.buf + m_up.pos) = htonl(length);

  m_up.buf[(m_up.pos += 4)++] = cmd;
  m_up.lastCommand = cmd;
}

void PeerConnection::bufW32(uint32_t v) {
  // TODO: Clean up when stable.
  if (m_up.pos + 4 > m_up.length)
    throw internal_error("PeerConnection tried to write beyond scope of packet");

  *(uint32_t*)&(m_up.buf[m_up.pos]) = htonl(v);

  m_up.pos += 4;
}

uint32_t PeerConnection::bufR32(bool peep) {
  unsigned int pos = m_down.pos;

  if (!peep && (m_down.pos += 4) > m_down.length)
    throw communication_error("PeerConnection tried to read beyond scope of packet");

  return ntohl(*(uint32_t*)&(m_down.buf[pos]));
}
 
void
PeerConnection::load_chunk(int index, Sub& sub) {
  if (sub.data.is_valid() && index == sub.data->get_index())
    return;

  if (index < 0 || index >= (signed)m_download->get_chunk_total())
    throw internal_error("Incoming pieces list contains a bad index value");
  
  sub.data = m_download->get_content().get_storage().get_chunk(index, true);
  
  if (!sub.data.is_valid())
    throw local_error("PeerConnection failed to get a chunk to write to");
}

bool
PeerConnection::request_piece() {
  const Piece* p;

  if ((p = m_requests.delegate()) == NULL)
    return false;

  if (p->get_length() > (1 << 17) ||
      p->get_length() == 0 ||
      p->get_length() + p->get_offset() >
      
      ((unsigned)p->get_index() + 1 != m_download->get_chunk_total() ||
       !(m_download->get_content().get_size() % m_download->get_content().get_storage().get_chunk_size()) ?
       
       m_download->get_content().get_storage().get_chunk_size() :
       (m_download->get_content().get_size() % m_download->get_content().get_storage().get_chunk_size()))) {
    
    std::stringstream s;
    
    s << "Tried to request a piece with invalid length or offset: "
      << p->get_length() << ' '
      << p->get_offset();
    
    throw internal_error(s.str());
  }

  if (p->get_index() < 0 ||
      p->get_index() >= (int)m_bitfield.size_bits() ||
      !m_bitfield[p->get_index()])
    throw internal_error("Delegator gave us a piece with invalid range or not in peer");

  bufCmd(REQUEST, 13);
  bufW32(p->get_index());
  bufW32(p->get_offset());
  bufW32(p->get_length());

  return true;
}

bool PeerConnection::chokeDelayed() {
  return m_sendChoked || m_taskSendChoke.is_scheduled();
}

void PeerConnection::choke(bool v) {
  if (m_up.choked != v) {
    m_sendChoked = true;
    m_up.choked = v;

    insert_write();
  }
}

void
PeerConnection::update_interested() {
  if (m_net->get_delegator().get_select().interested(m_bitfield.get_bitfield())) {
    m_sendInterested = !m_down.interested;
    m_down.interested = true;
  } else {
    m_sendInterested = m_down.interested;
    m_down.interested = false;
  }
}

} // namespace torrent
