#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cerrno>
#include <sstream>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <algo/algo.h>

#include "torrent/exceptions.h"
#include "download/download_net.h"

#include "download/download_state.h"
#include "peer_connection.h"
#include "general.h"
#include "settings.h"

// TODO: Put this somewhere better, make adjustable?
#define BUFFER_SIZE ((unsigned int)(1<<9))

using namespace algo;

namespace torrent {

// Find a better way to do this?
extern std::list<std::string> caughtExceptions;

PeerConnection*
PeerConnection::create(int fd, const PeerInfo& p, DownloadState* d, DownloadNet* net) {
  PeerConnection* pc = new PeerConnection;
  pc->set(fd, p, d, net);

  return pc;
}

void PeerConnection::set(int fd, const PeerInfo& p, DownloadState* d, DownloadNet* net) {
  if (m_fd >= 0)
    throw internal_error("Tried to re-set PeerConnection");

  set_socket_min_cost(m_fd);

  m_fd = fd;
  m_peer = p;
  m_download = d;
  m_net = net;

  m_requests.set_delegator(&m_net->get_delegator());
  m_requests.set_bitfield(&m_bitfield.get_bitfield());

  if (d == NULL ||
      !p.is_valid() ||
      m_fd < 0)
    throw internal_error("PeerConnection set recived bad input");

  // Set the bitfield size and zero it
  m_bitfield = BitFieldExt(d->get_chunk_total());

  if (m_bitfield.begin() == NULL)
    throw internal_error("PeerConnection::set(...) did not properly initialize m_bitfield"); 

  insert_read();
  insert_write();
  insert_except();

  m_up.buf = new uint8_t[BUFFER_SIZE];
  m_down.buf = new uint8_t[BUFFER_SIZE];

  if (!d->get_content().get_bitfield().all_zero()) {
    // Send bitfield to peer.
    bufCmd(BITFIELD, 1 + m_download->get_content().get_bitfield().size_bytes(), 1);
    m_up.pos = 0;
    m_up.state = WRITE_MSG;
  }
    
  insert_service(Timer::current() + 120 * 1000000, SERVICE_KEEP_ALIVE);

  m_lastMsg = Timer::current();
}

void PeerConnection::read() {
  Piece piece;
  int previous;
  bool s;

  try {
    
  evil_goto_read:
    
  switch (m_down.state) {
  case IDLE:
    m_down.pos = 0;
    m_down.state = READ_LENGTH;

  case READ_LENGTH:
    if (!read_buf(m_down.buf + m_down.pos, 4, m_down.pos))
      return;

    m_down.pos = 0;
    m_down.lengthOrig = m_down.length = bufR32(true);

    if (m_down.length == 0) {
      // Received ping.

      m_down.state = IDLE;
      m_down.lastCommand = KEEP_ALIVE;

      return;

    } else if (m_down.length > (1 << 17) + 9) {
      std::stringstream s;
      s << "Recived packet with length 0x" << std::hex << m_down.length;

      throw communication_error(s.str());
    }
    
    m_down.state = READ_TYPE;

    // TODO: Read up to 9 or something.
  case READ_TYPE:
    if (!read_buf(m_down.buf, 1, m_down.pos))
      return;

    switch (m_down.buf[0]) {
    case REQUEST:
    case CANCEL:
      if (m_down.length != 13)
	throw communication_error("Recived request/cancel command with wrong size");

      break;

    case HAVE:
      if (m_down.length != 5)
	throw communication_error("Recived have command with wrong size");
      
      break;

    case PIECE:
      if (m_down.length < 9 || m_down.length > 9 + (1 << 17))
	throw communication_error("Received piece message with bad length");

      m_down.length = 9;

      break;

    case BITFIELD:
      if (m_down.length != 1 + m_bitfield.size_bytes()) {
	std::stringstream s;

	s << "Recived bitfield message with wrong size " << m_down.length
	  << ' ' << m_bitfield.size_bytes() << ' ';

	throw communication_error(s.str());

      } else if (m_down.lastCommand != NONE) {
	throw communication_error("BitField received after other commands");
      }

      m_down.pos = 0;
      m_down.state = READ_BITFIELD;

      goto evil_goto_read;

    default:
      if ((unsigned char)m_down.buf[0] > CANCEL)
	throw communication_error("Received unknown protocol command");

      break;
    };

    m_down.state = READ_MSG;
    m_down.lastCommand = (Protocol)m_down.buf[0];

  case READ_MSG:
    if (m_down.length > 1 &&
	!read_buf(m_down.buf + m_down.pos, m_down.length, m_down.pos))
      return;

    switch (m_down.buf[0]) {
    case PIECE:
      if (m_down.lengthOrig == 9) {
	// Some clients send zero length messages when we request pieces
	// they don't have.
	caughtExceptions.push_front("Received piece with length zero");

	m_down.state = IDLE;
	goto evil_goto_read;
      }

      m_down.pos = 1;
      piece.set_index(bufR32());
      piece.set_offset(bufR32());
      piece.set_length(m_down.lengthOrig - 9);
      
      m_down.pos = 0;
      
      if (m_requests.downloading(piece)) {
	m_down.state = READ_PIECE;
	load_chunk(m_requests.get_piece().get_index(), m_down);

      } else {
	// We don't want the piece,
	m_down.length = piece.get_length();
	m_down.state = READ_SKIP_PIECE;

	caughtExceptions.push_back("Receiving piece we don't want from " + m_peer.get_dns());
      }

      goto evil_goto_read;

    default:
      m_down.pos = 1;
      parseReadBuf();
      
      m_down.state = IDLE;
      goto evil_goto_read;
    }

  case READ_BITFIELD:
    if (!read_buf(m_bitfield.begin() + m_down.pos, m_down.length - 1, m_down.pos))
      return;

    m_bitfield.update_count();

    if (!m_bitfield.all_zero() && m_net->get_delegator().get_select().interested(m_bitfield.get_bitfield())) {
      m_up.interested = m_sendInterested = true;
      
    } else if (m_bitfield.all_set() && m_download->get_content().is_done()) {
      // Both sides are done so we might as well close the connection.
      throw close_connection();
    }

    m_down.state = IDLE;
    m_download->get_bitfield_counter().inc(m_bitfield.get_bitfield());

    insert_write();
    goto evil_goto_read;

  case READ_PIECE:
    if (!m_requests.is_downloading())
      throw internal_error("READ_PIECE state but RequestList is not downloading");

    if (!m_requests.is_wanted()) {
      m_down.state = READ_SKIP_PIECE;
      m_down.length = m_requests.get_piece().get_length() - m_down.pos;
      m_down.pos = 0;

      m_requests.skip();

      goto evil_goto_read;
    }

    previous = m_down.pos;
    s = readChunk();

    m_throttle.down().add(m_down.pos - previous);
    m_net->get_rate_down().add(m_down.pos - previous);
    
    if (!s)
      return;

    m_down.state = IDLE;
    m_tryRequest = true;

    m_requests.finished();
    
    // TODO: Find a way to avoid this remove/insert cycle.
    remove_service(SERVICE_STALL);
    
    if (m_requests.get_size())
      insert_service(Timer::cache() + m_download->get_settings().stallTimeout, SERVICE_STALL);

    // TODO: clear m_down.data?

    insert_write();

    goto evil_goto_read;

  case READ_SKIP_PIECE:
    if (m_down.pos != 0)
      throw internal_error("READ_SKIP_PIECE m_down.pos != 0");

    s = read_buf(m_down.buf,
		 std::min(m_down.length, BUFFER_SIZE),
		 m_down.pos);

    m_throttle.down().add(m_down.pos);

    m_down.length -= m_down.pos;
    m_down.pos = 0;

    if (m_down.length == 0) {
      // Done with this piece.
      m_down.state = IDLE;
      m_tryRequest = true;

      remove_service(SERVICE_STALL);

      if (m_requests.get_size())
	insert_service(Timer::cache() + m_download->get_settings().stallTimeout, SERVICE_STALL);
    }

    if (s)
      goto evil_goto_read;
    else
      return;

  default:
    throw internal_error("peer_connectino::read() called on object in wrong state");
  }

  } catch (close_connection& e) {
    m_net->remove_connection(this);

  } catch (network_error& e) {
    caughtExceptions.push_front(e.what());

    m_net->remove_connection(this);

  } catch (base_error& e) {
    std::stringstream s;
    s << "Connection read fd(" << m_fd << ") state(" << m_down.state << ") \"" << e.what() << '"';

    e.set(s.str());

    throw;
  }
}

void PeerConnection::write() {
  bool s;
  int previous, maxBytes;

  m_lastMsg = Timer::cache();

  try {

  evil_goto_write:

  switch (m_up.state) {
  case IDLE:
    m_up.pos = 0;
    m_up.length = 0;

    // Keep alives must set the 5th bit to something IDLE

    if (m_shutdown)
      throw close_connection();

    fillWriteBuf();

    if (m_up.length == 0)
      return remove_write();

    m_up.state = WRITE_MSG;
    m_up.pos = 0;

  case WRITE_MSG:
    if (!write_buf(m_up.buf + m_up.pos, m_up.length, m_up.pos))
      return;

    m_up.pos = 0;

    switch (m_up.lastCommand) {
    case BITFIELD:
      m_up.state = WRITE_BITFIELD;

      goto evil_goto_write;

    case PIECE:
      // TODO: Do this somewhere else, and check to see if we are already using the right chunk
      if (m_sends.empty())
	throw internal_error("Tried writing piece without any requests in list");	  
	
      m_up.data = m_download->get_content().get_storage().get_chunk(m_sends.front().get_index());
      m_up.state = WRITE_PIECE;

      if (!m_up.data.is_valid())
	throw local_error("Failed to get a chunk to read from");

      goto evil_goto_write;
      
    default:
      m_up.state = IDLE;
      return;
    }

    m_up.pos = 0;

  case WRITE_BITFIELD:
    if (!write_buf(m_download->get_content().get_bitfield().begin() + m_up.pos,
		   m_download->get_content().get_bitfield().size_bytes(), m_up.pos))
      return;

    m_up.state = IDLE;
    return;

  case WRITE_PIECE:
    if (m_sends.empty())
      throw internal_error("WRITE_PIECE on an empty list");

    previous = m_up.pos;
    maxBytes = m_throttle.left();
    
    if (maxBytes == 0) {
      remove_write();
      return;
    }

    if (maxBytes < 0)
      throw internal_error("PeerConnection::write() got maxBytes <= 0");

    s = writeChunk(maxBytes);

    m_throttle.up().add(m_up.pos - previous);
    m_throttle.spent(m_up.pos - previous);

    m_net->get_rate_up().add(m_up.pos - previous);

    if (!s)
      return;

    if (m_sends.empty())
      m_up.data = Storage::Chunk();

    m_sends.pop_front();

    m_up.state = IDLE;
    return;

  default:
    throw internal_error("PeerConnection::write() called on object in wrong state");
  }

  } catch (close_connection& e) {
    m_net->remove_connection(this);

  } catch (network_error& e) {
    caughtExceptions.push_front(e.what());

    m_net->remove_connection(this);

  } catch (base_error& e) {
    std::stringstream s;
    s << "Connection write fd(" << m_fd << ") state(" << m_up.state << ") \"" << e.what() << '"';

    e.set(s.str());

    throw;
  }
}

void PeerConnection::except() {
  caughtExceptions.push_front("Connection exception: " + std::string(strerror(errno)));

  m_net->remove_connection(this);
}

void PeerConnection::parseReadBuf() {
  uint32_t index, offset, length;
  SendList::iterator rItr;

  switch (m_down.buf[0]) {
  case CHOKE:
    m_down.choked = true;
    m_requests.cancel();

    remove_service(SERVICE_STALL);

    return;

  case UNCHOKE:
    m_down.choked = false;
    m_tryRequest = true;
    
    return insert_write();

  case INTERESTED:
    m_down.interested = true;

    // If we want to send stuff.
    if (m_up.choked &&
	m_net->can_unchoke() > 0) {
      choke(false);
    }
    
    return;

  case NOT_INTERESTED:
    m_down.interested = false;

    // Choke this uninterested peer and unchoke someone else.
    if (!m_up.choked) {
      choke(true);

      m_net->choke_balance();
    }

    return;

  case REQUEST:
  case CANCEL:
    if (m_up.choked)
      return;

    index = bufR32();
    offset = bufR32();
    length = bufR32();

    rItr = std::find(m_sends.begin(), m_sends.end(),
		     Piece(index, offset, length));
      
    if (m_down.buf[0] == REQUEST) {
      if (rItr != m_sends.end())
	m_sends.erase(rItr);
      
      m_sends.push_back(Piece(index, offset, length));
      insert_write();

    } else if (rItr != m_sends.end()) {

      // Only cancel if we're not writing it.
      if (rItr != m_sends.begin() || m_up.lastCommand != PIECE || m_up.state == IDLE)
	m_sends.erase(rItr);
    }

    return insert_write();

  case HAVE:
    index = bufR32();

    if (index >= m_bitfield.size_bits())
      throw communication_error("Recived HAVE command with invalid value");

    if (!m_bitfield[index]) {
      m_bitfield.set(index, true);
      m_download->get_bitfield_counter().inc(index);
    }
    
    if (!m_up.interested && m_net->get_delegator().get_select().interested(index)) {
      // We are interested, send flag if not already set.
      m_sendInterested = !m_up.interested;
      m_up.interested = true;
      m_tryRequest = true;
	
      insert_write();
    }

    m_ratePeer.add(m_download->get_content().get_storage().get_chunksize());

    return;

  default:
    // TODO: this is a communication error.
    throw internal_error("peer sent unsupported command");
  };
}

void PeerConnection::fillWriteBuf() {
  if (m_sendChoked) {
    m_sendChoked = false;

    if ((Timer::cache() - m_lastChoked).seconds() < 10) {
      // Wait with the choke message.
      insert_service(m_lastChoked + 10 * 1000000,
		    SERVICE_SEND_CHOKE);

    } else {
      // CHOKE ME
      bufCmd(m_up.choked ? CHOKE : UNCHOKE, 1);
      
      m_lastChoked = Timer::cache();

      if (m_up.choked) {
	// Clear the request queue and mmaped chunk.
	m_sends.clear();
	m_up.data = Storage::Chunk();
	
	m_throttle.idle();
	
      } else {
	m_throttle.activate();
      }
    }
  }

  if (m_sendInterested) {
    bufCmd(m_up.interested ? INTERESTED : NOT_INTERESTED, 1);

    m_sendInterested = false;
  }

  uint32_t pipeSize;

  if (m_tryRequest && !m_down.choked && m_up.interested &&

      m_down.state != READ_SKIP_PIECE &&
      m_net->should_request(m_stallCount) &&
      m_requests.get_size() < (pipeSize = m_net->pipe_size(m_throttle.down()))) {

    m_tryRequest = false;

    while (m_requests.get_size() < pipeSize && m_up.length + 16 < BUFFER_SIZE && request_piece())

      if (m_requests.get_size() == 1) {
	if (in_service(SERVICE_STALL))
	  throw internal_error("Only one request, but we're already in SERVICE_STALL");
	
	m_tryRequest = true;
	insert_service(Timer::cache() + m_download->get_settings().stallTimeout, SERVICE_STALL);
      }	
  }

  // Max buf size 17 * 'req pipe' + 10

  while (!m_haveQueue.empty() &&
	 m_up.length + 9 < BUFFER_SIZE) {
    bufCmd(HAVE, 5);
    bufW32(m_haveQueue.front());

    m_haveQueue.pop_front();
  }

  if (!m_up.choked &&
      !m_sendChoked &&
      !m_sends.empty() &&
      m_up.length + 13 < BUFFER_SIZE) {
    // Sending chunk to peer.

    // This check takes care of all possible errors in lenght and offset.
    if (m_sends.front().get_length() > (1 << 17) ||
	m_sends.front().get_length() == 0 ||

	m_sends.front().get_length() + m_sends.front().get_offset() >
	m_download->get_content().get_chunksize(m_sends.front().get_index())) {

      std::stringstream s;

      s << "Peer requested a piece with invalid length or offset: "
	<< m_sends.front().get_length() << ' '
	<< m_sends.front().get_offset();

      throw communication_error(s.str());
    }
      
    if (m_sends.front().get_index() < 0 ||
	m_sends.front().get_index() >= (signed)m_download->get_chunk_total() ||
	!m_download->get_content().get_bitfield()[m_sends.front().get_index()]) {
      std::stringstream s;

      s << "Peer requested a piece with invalid index: " << m_sends.front().get_index();

      throw communication_error(s.str());
    }

    bufCmd(PIECE, 9 + m_sends.front().get_length(), 9);
    bufW32(m_sends.front().get_index());
    bufW32(m_sends.front().get_offset());
  }
}

void PeerConnection::sendHave(int index) {
  m_haveQueue.push_back(index);

  if (m_download->get_content().is_done()) {
    // We're done downloading.

    if (m_bitfield.all_set()) {
      // Peer is done, close connection.
      m_shutdown = true;

    } else {
      m_sendInterested = m_up.interested;
      m_up.interested = false;
    }

  } else if (m_up.interested && !m_net->get_delegator().get_select().interested(m_bitfield.get_bitfield())) {
    // TODO: Optimize?
    m_sendInterested = true;
    m_up.interested = false;
  }

  if (m_requests.has_index(index))
    throw internal_error("PeerConnection::sendHave(...) found a request with the same index");

  // TODO: Also send cancel messages!

  // TODO: Remove this so we group the have messages with other stuff.
  insert_write();
}

void PeerConnection::service(int type) {
  switch (type) {
  case SERVICE_KEEP_ALIVE:
    if ((Timer::cache() - m_lastMsg).usec() > 150 * 1000000) {
      m_net->remove_connection(this);
      return;
    }

    if (m_up.state == IDLE) {
      m_up.length = 0;

      bufCmd(KEEP_ALIVE, 0);

      m_up.state = WRITE_MSG;
      m_up.pos = 0;

      insert_write();
    }

    m_tryRequest = true;
    insert_service(Timer::cache() + 120 * 1000000, SERVICE_KEEP_ALIVE);

    return;
  
  case SERVICE_SEND_CHOKE:
    m_sendChoked = true;

    insert_write();
    return;
    
  case SERVICE_STALL:
    m_stallCount++;
    m_requests.stall();

    // Make sure we regulary call SERVICE_STALL so stalled queues with new
    // entries get those new ones stalled if needed.
    insert_service(Timer::cache() + m_download->get_settings().stallTimeout, SERVICE_STALL);

    caughtExceptions.push_back("Peer stalled " + m_peer.get_dns());
    return;

  default:
    throw internal_error("PeerConnection::service received bad type");
  };
}

}

