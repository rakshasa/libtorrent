#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <unistd.h>
#include <algo/algo.h>
#include <netinet/in.h>

#include "peer_connection.h"
#include "torrent/exceptions.h"
#include "download_state.h"

#define BUFFER_SIZE (1<<9)

using namespace algo;

// This file is for functions that does not directly relate to the
// protocol logic. Or the split might be completely arbitary...

namespace torrent {

PeerConnection::PeerConnection() :
  m_fd(-1),
  m_shutdown(false),

  m_download(NULL),

  m_sendChoked(false),
  m_sendInterested(false)
{
}

PeerConnection::~PeerConnection() {
  if (m_download) {
    discardIncomingQueue();

    if (m_down.state != READ_BITFIELD)
      m_download->bfCounter().dec(m_bitfield);
  }

  delete [] m_up.buf;
  delete [] m_down.buf;

  if (m_fd >= 0)
    close(m_fd);
}

int PeerConnection::fd() {
  return m_fd;
}

bool PeerConnection::writeChunk(int maxBytes) {
  StorageChunk::Node& part = m_up.data->get_position(m_up.list.front().offset() + m_up.pos);

  unsigned int length = std::min(m_up.list.front().length(),
				 part.position + part.length - m_up.list.front().offset());

  // TODO: Make this a while loop so we spit out as much of the piece as we can this work cycle.

  writeBuf(part.chunk.begin() + m_up.list.front().offset() + m_up.pos - part.position,
	   std::min(length, m_up.pos + maxBytes),
	   m_up.pos);

  return m_up.pos == m_up.list.front().length();
}

bool PeerConnection::readChunk() {
  if (m_down.pos > (1 << 17) + 9)
    throw internal_error("Really bad read position for buffer");
  
  StorageChunk::Node& part = m_down.data->get_position(m_down.list.front().offset() + m_down.pos);

  unsigned int offset = m_down.list.front().offset() + m_down.pos - part.position;
  
  if (!part.chunk.is_valid() ||
      !part.chunk.is_write())
    throw storage_error("Tried to write piece to file area that isn't valid or can't be written to");
  
  if (!readBuf(part.chunk.begin() + offset,
	       std::min(part.position + part.chunk.length() - m_down.list.front().offset(),
			m_down.list.front().length()),
	       m_down.pos))
    return false; // Did not read the whole part of the piece
  
  if (m_down.pos != m_down.list.front().length())
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
 
void PeerConnection::discardIncomingQueue() {
  std::for_each(m_down.list.begin(), m_down.list.end(),
		call_member(ref(m_download->delegator()),
			    &Delegator::cancel,
			    
			    ref(m_peer.id()),
			    back_as_ref(),
			    value(true)));

  // TODO, don't clear list here.(?)
  m_down.list.clear();
  m_down.data = Storage::Chunk();
}

bool PeerConnection::chokeDelayed() {
  return m_sendChoked || in_service(SERVICE_SEND_CHOKE);
}

void PeerConnection::choke(bool v) {
  if (m_up.choked != v) {
    m_sendChoked = true;
    m_up.choked = v;

    insertWrite();
  }
}

} // namespace torrent
