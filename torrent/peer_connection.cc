#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cerrno>
#include <sstream>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <algo/algo.h>

#include "torrent/exceptions.h"
#include "download_state.h"
#include "peer_connection.h"
#include "general.h"

// TODO: Put this somewhere better, make adjustable?
#define BUFFER_SIZE ((unsigned int)(1<<9))

using namespace algo;

namespace torrent {

// Find a better way to do this?
extern std::list<std::string> caughtExceptions;

void PeerConnection::set(int fd, const PeerInfo& p, DownloadState* d) {
  if (m_fd >= 0)
    throw internal_error("Tried to re-set PeerConnection");

  m_requests.set_delegator(&d->delegator());
  m_requests.set_bitfield(&m_bitfield);

  set_socket_min_cost(m_fd);

  m_fd = fd;
  m_peer = p;
  m_download = d;

  if (d == NULL ||
      !p.is_valid() ||
      m_fd < 0)
    throw internal_error("PeerConnection set recived bad input");

  // Set the bitfield size and zero it
  m_bitfield = BitField(d->content().get_storage().get_chunkcount());

  insertRead();
  insertWrite();
  insertExcept();

  makeBuf(&m_up.buf, BUFFER_SIZE);
  makeBuf(&m_down.buf, BUFFER_SIZE);

  if (!d->content().get_bitfield().zero()) {
    // Send bitfield to peer.
    bufCmd(BITFIELD, 1 + m_download->content().get_bitfield().sizeBytes(), 1);
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
    if (!readBuf(m_down.buf + m_down.pos, 4, m_down.pos))
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
    if (!readBuf(m_down.buf, 1, m_down.pos))
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
      if (m_down.length != 1 + m_bitfield.sizeBytes()) {
	std::stringstream s;

	s << "Recived bitfield message with wrong size " << m_down.length
	  << ' ' << m_bitfield.sizeBytes() << ' ';

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
	!readBuf(m_down.buf + m_down.pos, m_down.length, m_down.pos))
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
      piece.index() = bufR32();
      piece.offset() = bufR32();
      piece.length() = m_down.lengthOrig - 9;
      
      m_down.pos = 0;
      
      if (m_requests.downloading(piece)) {
	m_down.state = READ_PIECE;
	load_chunk(m_requests.get_piece().get_index(), m_down);

      } else {
	// We don't want the piece,
	m_down.length = piece.get_length();
	m_down.state = READ_SKIP_PIECE;

	remove_service(SERVICE_STALL);
	caughtExceptions.push_back("Receiving piece we don't want from " + m_peer.dns());
      }

      goto evil_goto_read;

    default:
      m_down.pos = 1;
      parseReadBuf();
      
      m_down.state = IDLE;
      goto evil_goto_read;
    }

  case READ_BITFIELD:
    if (!readBuf(m_bitfield.data() + m_down.pos, m_down.length - 1, m_down.pos))
      return;

    if (m_download->delegator().interested(m_bitfield)) {
      m_up.interested = m_sendInterested = true;

    } else if (m_download->content().get_completed() == m_download->content().get_storage().get_chunkcount() &&
	       m_bitfield.allSet()) {
      // Both sides are done so we might as well close the connection.
      throw close_connection();
    }

    m_down.state = IDLE;
    m_download->bfCounter().inc(m_bitfield);

    insertWrite();
    goto evil_goto_read;

  case READ_PIECE:
    if (!m_requests.is_downloading())
      throw internal_error("READ_PIECE state but RequestList is not downloading");

    // TODO: Temporary, kill as soon as possible.
    if (!in_service(SERVICE_STALL))
      throw internal_error("READ_PIECE state but peer not in SERVICE_STALL");

    previous = m_down.pos;
    s = readChunk();

    m_throttle.down().add(m_down.pos - previous);
    m_download->rateDown().add(m_down.pos - previous);
    
    if (!s)
      return;

    m_down.state = IDLE;
    m_download->bytesDownloaded() += m_requests.get_piece().get_length();

    if (m_requests.finished())
      m_download->chunkDone(m_down.data);
    
    remove_service(SERVICE_STALL);
    
    if (m_requests.get_size())
      insert_service(Timer::cache() + m_download->settings().stallTimeout, SERVICE_STALL);

    // TODO: clear m_down.data?

    insertWrite();

    goto evil_goto_read;

  case READ_SKIP_PIECE:
    // TODO: Temporary, kill as soon as possible.
    if (in_service(SERVICE_STALL))
      throw internal_error("READ_SKIP_PIECE state but peer is in SERVICE_STALL");

    previous = m_down.pos;
    s = readBuf(m_down.buf,
		std::min(m_down.length, BUFFER_SIZE),
		m_down.pos);

    m_throttle.down().add(m_down.pos - previous);

    if (!s)
      return;
    
    m_down.length -= m_down.pos;
    m_down.pos = 0;

    if (m_down.length == 0) {
      // Done with this piece.
      m_down.state = IDLE;

      if (m_requests.get_size())
	insert_service(Timer::cache() + m_download->settings().stallTimeout, SERVICE_STALL);
    }

    goto evil_goto_read;

  default:
    throw internal_error("peer_connectino::read() called on object in wrong state");
  }

  } catch (close_connection& e) {
    m_download->removeConnection(this);

  } catch (network_error& e) {
    caughtExceptions.push_front(e.what());

    m_download->removeConnection(this);

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
      return removeWrite();

    m_up.state = WRITE_MSG;
    m_up.pos = 0;

  case WRITE_MSG:
    if (!writeBuf(m_up.buf + m_up.pos, m_up.length, m_up.pos))
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
	
      m_up.data = m_download->content().get_storage().get_chunk(m_sends.front().index());
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
    if (!writeBuf(m_download->content().get_bitfield().data() + m_up.pos,
		  m_download->content().get_bitfield().sizeBytes(), m_up.pos))
      return;

    m_up.state = IDLE;
    return;

  case WRITE_PIECE:
    if (m_sends.empty())
      throw internal_error("WRITE_PIECE on an empty list");

    previous = m_up.pos;
    maxBytes = m_throttle.left();
    
    if (maxBytes == 0) {
      removeWrite();
      return;
    }

    if (maxBytes < 0)
      throw internal_error("PeerConnection::write() got maxBytes <= 0");

    s = writeChunk(maxBytes);

    m_throttle.up().add(m_up.pos - previous);
    m_throttle.spent(m_up.pos - previous);

    m_download->rateUp().add(m_up.pos - previous);

    if (!s)
      return;
    
    m_download->bytesUploaded() += m_sends.front().length();

    if (m_sends.empty())
      m_up.data = Storage::Chunk();

    m_sends.pop_front();

    m_up.state = IDLE;
    return;

  default:
    throw internal_error("PeerConnection::write() called on object in wrong state");
  }

  } catch (close_connection& e) {
    m_download->removeConnection(this);

  } catch (network_error& e) {
    caughtExceptions.push_front(e.what());

    m_download->removeConnection(this);

  } catch (base_error& e) {
    std::stringstream s;
    s << "Connection write fd(" << m_fd << ") state(" << m_up.state << ") \"" << e.what() << '"';

    e.set(s.str());

    throw;
  }
}

void PeerConnection::except() {
  caughtExceptions.push_front("Connection exception: " + std::string(strerror(errno)));

  m_download->removeConnection(this);
}

void PeerConnection::parseReadBuf() {
  uint32_t index, offset, length;
  SendList::iterator rItr;

  switch (m_down.buf[0]) {
  case CHOKE:
    m_down.choked = true;
    insert_service(Timer::cache() + m_download->settings().cancelTimeout, SERVICE_CANCEL);

    return;

  case UNCHOKE:
    m_down.choked = false;
    remove_service(SERVICE_CANCEL);
    
    return insertWrite();

  case INTERESTED:
    m_down.interested = true;

    // If we want to send stuff.
    if (m_up.choked &&
	m_download->canUnchoke() > 0) {
      choke(false);
    }
    
    return;

  case NOT_INTERESTED:
    m_down.interested = false;

    // Choke this uninterested peer and unchoke someone else.
    if (!m_up.choked) {
      choke(true);

      m_download->chokeBalance();
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
      insertWrite();

    } else if (rItr != m_sends.end()) {

      // Only cancel if we're not writing it.
      if (rItr != m_sends.begin() || m_up.lastCommand != PIECE || m_up.state == IDLE)
	m_sends.erase(rItr);
    }

    return insertWrite();

  case HAVE:
    index = bufR32();

    if (index >= m_bitfield.sizeBits())
      throw communication_error("Recived HAVE command with invalid value");

    if (!m_bitfield[index]) {
      m_bitfield.set(index);
      m_download->bfCounter().inc(index);
    }
    
    if (!m_up.interested &&
	m_download->delegator().interested(index)) {
      // We are interested, send flag if not already set.
      m_sendInterested = !m_up.interested;
      m_up.interested = true;

      insertWrite();
    }

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

  if (!m_down.choked && m_up.interested && !m_stallCount && m_down.state != READ_SKIP_PIECE) {

    unsigned int ps = m_download->pipe_size().calculate(m_throttle.down());

    while (m_requests.get_size() < ps && m_up.length + 16 < BUFFER_SIZE && request_piece())
      if (m_requests.get_size() == 1) {
	if (in_service(SERVICE_STALL))
	  throw internal_error("Only one request, but we're already in SERVICE_STALL");
	
	insert_service(Timer::cache() + m_download->settings().stallTimeout, SERVICE_STALL);
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
    if (m_sends.front().length() > (1 << 17) ||
	m_sends.front().length() == 0 ||
	m_sends.front().length() + m_sends.front().offset() >

	((unsigned)m_sends.front().index() + 1 != m_download->content().get_storage().get_chunkcount()  ||
	 !(m_download->content().get_size() % m_download->content().get_storage().get_chunksize()) ?

	 m_download->content().get_storage().get_chunksize() :
	 (m_download->content().get_storage().get_size() % m_download->content().get_storage().get_chunksize()))) {

      std::stringstream s;

      s << "Peer requested a piece with invalid length or offset: "
	<< m_sends.front().length() << ' '
	<< m_sends.front().offset();

      throw communication_error(s.str());
    }
      
    if (m_sends.front().index() < 0 ||
	m_sends.front().index() >= (signed)m_download->content().get_storage().get_chunkcount() ||
	!m_download->content().get_bitfield()[m_sends.front().index()]) {
      std::stringstream s;

      s << "Peer requested a piece with invalid index: " << m_sends.front().index();

      throw communication_error(s.str());
    }

    bufCmd(PIECE, 9 + m_sends.front().length(), 9);
    bufW32(m_sends.front().index());
    bufW32(m_sends.front().offset());
  }
}

void PeerConnection::sendHave(int index) {
  m_haveQueue.push_back(index);

  if (m_download->content().get_completed() == m_download->content().get_storage().get_chunkcount()) {
    // We're done downloading.

    if (m_bitfield.allSet()) {
      // Peer is done, close connection.
      m_shutdown = true;

    } else {
      m_sendInterested = m_up.interested;
      m_up.interested = false;
    }

  } else if (m_up.interested &&
	     !m_download->delegator().interested(m_bitfield)) {
    m_sendInterested = true;
    m_up.interested = false;
  }

  // Clear all pieces with this index and if one is being downloaded then
  // skip it.
  if (m_requests.is_downloading() &&
      m_requests.get_piece().get_index() == index) {
    throw internal_error("PeerConnection::sendHave(...) got RequestList.is_downloading() with this index");

    if (m_down.state != READ_PIECE)
      throw internal_error("PeerConnection::sendHave(...) got RequestList.is_downloading() but state is not READ_PIECE");

    m_down.state = READ_SKIP_PIECE;
    m_down.length = m_requests.get_piece().get_length() - m_down.pos;
    m_down.pos = 0;

    // TODO: Do some proper canceling here.

    m_down.data = Storage::Chunk();
  }

  if (m_requests.has_index(index))
    throw internal_error("PeerConnection::sendHave(...) found a request with the same index");

  // TODO: Also send cancel messages!

  insertWrite();
}

void PeerConnection::service(int type) {
  switch (type) {
  case SERVICE_KEEP_ALIVE:
    if ((Timer::cache() - m_lastMsg).usec() > 150 * 1000000) {
      m_download->removeConnection(this);
      return;
    }

    if (m_up.state == IDLE) {
      m_up.length = 0;

      bufCmd(KEEP_ALIVE, 0);

      m_up.state = WRITE_MSG;
      m_up.pos = 0;

      insertWrite();
    }

    insert_service(Timer::cache() + 120 * 1000000, SERVICE_KEEP_ALIVE);

    return;
  
  case SERVICE_SEND_CHOKE:
    m_sendChoked = true;

    insertWrite();
    return;
    
  case SERVICE_STALL:
    // Clear the incoming queue reservations so we can request from other peers.
    if (m_down.state == READ_PIECE) {
      m_down.state = READ_SKIP_PIECE;
      m_down.length = m_requests.get_piece().get_length() - m_down.pos;
      m_down.pos = 0;
    }

    m_requests.stall();
    m_stallCount++;

    caughtExceptions.push_back("Peer stalled " + m_peer.dns());
    return;

  case SERVICE_CANCEL:
    if (!m_down.choked)
      return;

    if (m_down.state == READ_PIECE)
      throw communication_error("Peer choke while downloading piece");

    remove_service(SERVICE_STALL);
    m_requests.cancel();

    return;

  default:
    throw internal_error("PeerConnection::service received bad type");
  };
}

}

