#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "exceptions.h"
#include "download.h"
#include "peer_connection.h"
#include "general.h"

#include <sstream>
#include <netinet/in.h>
#include <algo/algo.h>

#define BUFFER_SIZE (1<<9)

using namespace algo;

namespace torrent {

// Find a better way to do this?
extern std::list<std::string> caughtExceptions;

void PeerConnection::set(int fd, const Peer& p, Download* d) {
  if (m_fd >= 0)
    throw internal_error("Tried to re-set PeerConnection");

  m_fd = fd;
  m_peer = p;
  m_download = d;

  if (d == NULL ||
      !p.valid() ||
      m_fd < 0)
    throw internal_error("PeerConnection set recived bad input");

  // Set the bitfield size and zero it
  m_bitfield = BitField(d->files().chunkCount());

  insertRead();
  insertWrite();
  insertExcept();

  makeBuf(&m_up.buf, BUFFER_SIZE);
  makeBuf(&m_down.buf, BUFFER_SIZE);

  if (!d->files().bitfield().zero()) {
    // Send bitfield to peer.
    bufCmd(BITFIELD, 1 + m_download->files().bitfield().sizeBytes(), 1);
    m_up.pos = 0;
    m_up.state = WRITE_MSG;
  }
    
  insertService(Timer::current() + 120 * 1000000, SERVICE_KEEP_ALIVE);

  m_lastChoked = Timer::current() - 10 * 1000000; // 10s
  m_lastMsg = Timer::current();
}

void PeerConnection::read() {
  PieceList::iterator pItr;
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
      if (m_down.length <= 9 || m_down.length > 9 + (1 << 17))
	throw communication_error("Recived piece message with bad length");

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
      m_down.pos = 1;
      piece.index() = bufR32();
      piece.offset() = bufR32();
      piece.length() = m_down.lengthOrig - 9;
      
      m_down.pos = 0;
      
      pItr = std::find(m_down.list.begin(), m_down.list.end(), piece);

      if (pItr == m_down.list.end()) {

	if (!m_download->files().bitfield()[piece.index()] &&
	    m_download->delegator().downloading(m_peer.id(), piece)) {
	  //std::cout << m_fd << " -> Receiving piece we don't have in queue, but still want" << std::endl;

	  m_down.list.push_front(piece);
	  m_down.state = READ_PIECE;

	} else {
	  //std::cout << m_fd << " -> Receiving piece we really don't want" << std::endl;
	  
	  m_down.length = m_down.lengthOrig - 9;
	  m_down.state = READ_SKIP_PIECE;
	}

      } else if (!m_download->delegator().downloading(m_peer.id(), piece)) {
	//std::cout << " -> Piece in queue but we don't want it" << std::endl;

	m_down.list.erase(pItr);

	m_down.length = piece.length();
	m_down.state = READ_SKIP_PIECE;

      } else {
	// TODO: Keep around all the requests, and erase all those the peer
	// skips. This will protect against a peer sending unrequested pieces.

	if (pItr != m_down.list.begin()) {
	  //std::cout << m_fd << " -> Sender skipped a few pieces" << std::endl;

	  std::for_each(m_down.list.begin(), pItr,
			call_member(ref(m_download->delegator()),
				    &Delegator::cancel,

				    ref(m_peer.id()),
				    back_as_ref(),
				    value(true)));

	  m_down.list.erase(m_down.list.begin(), pItr);
	}

	m_down.state = READ_PIECE;
      }
      
      // TODO: Need to handle the getChunk stuff in some better place.
      if (m_down.state == READ_PIECE &&
	  !m_down.list.empty() &&
	  m_down.data.index() != m_down.list.front().index())

	if (m_down.list.front().index() >= 0 && m_down.list.front().index() < (signed)m_bitfield.sizeBits())
	  m_down.data = m_download->files().getChunk(m_down.list.front().index(), true);
	else
	  throw internal_error("Incoming pieces list contains a bad index value");
      
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

    } else if (m_download->files().chunkCompleted() == m_download->files().chunkCount() &&
	       m_bitfield.allSet()) {
      // Both sides are done so we might as well close the connection.
      throw close_connection();
    }

    m_down.state = IDLE;
    m_download->delegator().bfCounter().inc(m_bitfield);

    insertWrite();
    goto evil_goto_read;

  case READ_PIECE:
    if (m_down.list.empty())
      throw internal_error("READ_PIECE on an empty list");

    // TODO: Move this somewhere else
    if (!m_download->delegator().downloading(m_peer.id(), m_down.list.front())) {
      // Piece finished by someone else, skip.
      m_down.length = m_down.list.front().length() - m_down.pos;
      m_down.pos = 0;
      m_down.list.pop_front();
      m_down.state = READ_SKIP_PIECE;

      return;
    }

    previous = m_down.pos;
    s = readChunk();

    m_down.rate.add(m_down.pos - previous);
    m_download->rateDown().add(m_down.pos - previous);
    
    if (!s) {
      return;
    }

    m_down.state = IDLE;
    m_download->m_bytesDownloaded += m_down.list.front().length();

    if (m_download->delegator().finished(m_peer.id(), m_down.list.front())) {
      // chunkDone pops the m_down.list
      m_download->chunkDone(m_down.data);
    } else {
      m_down.list.pop_front();
    }
    
    removeService(SERVICE_INCOMING_PIECE);
    
    if (!m_down.list.empty())
      insertService(Timer::cache() + 10 * 1000000, SERVICE_INCOMING_PIECE);

    // TODO: clear m_down.data?

    insertWrite();

    goto evil_goto_read;

  case READ_SKIP_PIECE:
    previous = m_down.pos;
    s = readBuf(m_down.buf,
		m_down.length > BUFFER_SIZE ?
		BUFFER_SIZE : m_down.length,
		m_down.pos);

    m_down.rate.add(m_down.pos - previous);

    if (!s)
      return;
    
    m_down.length -= m_down.pos;
    m_down.pos = 0;

    if (m_down.length == 0) {
      m_down.state = IDLE;
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

    throw e;
  }
}

void PeerConnection::write() {
  bool s;
  int previous;

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
      if (m_up.list.empty())
	throw internal_error("Tried writing piece without any requests in list");	  
	
      if (m_up.list.front().index() < 0 ||
	  (unsigned)m_up.list.front().index() >= m_bitfield.sizeBits())
	throw internal_error("Tried writing piece with bad index");

      m_up.data = m_download->files().getChunk(m_up.list.front().index());
      
      if (m_up.data.length() < m_up.list.front().offset() + m_up.list.front().length())
	throw communication_error("Tried to process request we don't have a valid chunk for");
      
      m_up.state = WRITE_PIECE;
      goto evil_goto_write;
      
    default:
      m_up.state = IDLE;
      return;
    }

    m_up.pos = 0;

  case WRITE_BITFIELD:
    if (!writeBuf(m_download->files().bitfield().data() + m_up.pos,
		  m_download->files().bitfield().sizeBytes(), m_up.pos))
      return;

    m_up.state = IDLE;
    return;

  case WRITE_PIECE:
    if (m_up.list.empty())
      throw internal_error("WRITE_PIECE on an empty list");

    previous = m_up.pos;
    s = writeChunk();

    m_up.rate.add(m_up.pos - previous);
    m_download->rateUp().add(m_up.pos - previous);

    if (!s)
      return;
    
    m_download->m_bytesUploaded += m_up.list.front().length();

    if (m_up.list.empty())
      m_up.data = Chunk();

    m_up.list.pop_front();

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

    throw e;
  }
}

void PeerConnection::except() {
  caughtExceptions.push_front("Connection exception: " + std::string(strerror(errno)));

  m_download->removeConnection(this);
}

void PeerConnection::parseReadBuf() {
  uint32_t index, offset, length;
  PieceList::iterator rItr;

  switch (m_down.buf[0]) {
  case CHOKE:
    m_down.choked = true;
    discardIncomingQueue();
    
    return;

  case UNCHOKE:
    m_down.choked = false;
    
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

    rItr = std::find(m_up.list.begin(), m_up.list.end(),
		     Piece(index, offset, length));
      
    if (m_down.buf[0] == REQUEST) {
      // TODO: Is this really an exception? what if we're choking abit?
      if (rItr != m_up.list.end())
	throw communication_error("Tried to request piece that has already been queued");
      
      m_up.list.push_back(Piece(index, offset, length));
      insertWrite();

    } else if (rItr != m_up.list.end()) {

      // TODO: Cancel if we're not writing it.
      if (rItr != m_up.list.begin())
	// Only cancel it if we're not writing it out at the moment.
	m_up.list.erase(rItr);
    }

    return insertWrite();

  case HAVE:
    index = bufR32();

    if (index >= m_bitfield.sizeBits())
      throw communication_error("Recived HAVE command with invalid value");

    if (!m_bitfield[index]) {
      m_bitfield.set(index);
      m_download->delegator().bfCounter().inc(index);
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
    throw internal_error("peer sendt unsupported command");
  };
}

void PeerConnection::fillWriteBuf() {
  if (m_sendChoked) {
    m_sendChoked = false;

    if ((Timer::cache() - m_lastChoked).seconds() < 10) {
      // Wait with the choke message.
      insertService(m_lastChoked + 10 * 1000000,
		    SERVICE_SEND_CHOKE);

    } else {
      // CHOKE ME
      bufCmd(m_up.choked ? CHOKE : UNCHOKE, 1);
      
      m_lastChoked = Timer::cache();
      
      if (m_up.choked) {
	// Clear the request queue and mmaped chunk.
	m_up.list.clear();
	m_up.data = Chunk();
      }
    }
  }

  if (m_sendInterested) {
    bufCmd(m_up.interested ? INTERESTED : NOT_INTERESTED, 1);

    m_sendInterested = false;
  }

  if (!m_down.choked && m_up.interested) {
    // Let us request more chunks.

    while (m_down.list.size() < 10 &&
	   m_download->delegator().delegate(m_peer.id(), m_bitfield, m_down.list)) {

      if (m_down.list.empty())
	insertService(Timer::cache() + 10 * 1000000, SERVICE_INCOMING_PIECE);

      bufCmd(REQUEST, 13);
      bufW32(m_down.list.back().index());
      bufW32(m_down.list.back().offset());
      bufW32(m_down.list.back().length());
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
      !m_up.list.empty() &&
      m_up.length + 13 < BUFFER_SIZE) {
    // Sending chunk to peer.

    if (m_up.list.front().length() > (1 << 17) ||
	(unsigned)m_up.list.front().index() >= m_bitfield.sizeBits() ||
	!m_download->files().bitfield()[m_up.list.front().index()])
      throw communication_error("Peer requested a bad piece");

    bufCmd(PIECE, 9 + m_up.list.front().length(), 9);
    bufW32(m_up.list.front().index());
    bufW32(m_up.list.front().offset());
  }
}

void PeerConnection::sendHave(int index) {
  m_haveQueue.push_back(index);

  if (m_download->files().chunkCompleted() == m_download->files().chunkCount()) {
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
  if (m_down.state == READ_PIECE &&
      !m_down.list.empty() &&
      m_down.list.front().index() == index) {

    m_down.state = READ_SKIP_PIECE;
    m_down.length = m_down.list.front().length() - m_down.pos;
    m_down.pos = 0;

    m_down.data = Chunk();
  }

  PieceList::iterator itr = m_down.list.begin();

  do {
    itr = std::find_if(itr, m_down.list.end(),
		       eq(call_member(&Piece::index), value(index)));

    if (itr == m_down.list.end())
      break;
    else
      itr = m_down.list.erase(itr);
  } while(true);

  // TODO: Also send cancel messages!

  insertWrite();
}

void PeerConnection::choke(bool v) {
  if (m_up.choked != v) {
    m_sendChoked = true;
    m_up.choked = v;

    insertWrite();
  }
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

    insertService(Timer::cache() + 120 * 1000000, SERVICE_KEEP_ALIVE);

    return;
  
  case SERVICE_SEND_CHOKE:
    m_sendChoked = true;

    insertWrite();
    return;
    
  case SERVICE_INCOMING_PIECE:
    // Clear the incoming queue reservations so we can request from other peers.
    std::for_each(m_down.list.begin(), m_down.list.end(),
		  call_member(ref(m_download->delegator()),
			      &Delegator::cancel,
			      
			      ref(m_peer.id()),
			      back_as_ref(),
			      value(false)));

    return;
  default:
    throw internal_error("PeerConnection::service received bad type");
  };
}

}

