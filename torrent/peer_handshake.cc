#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "general.h"
#include "exceptions.h"
#include "download.h"
#include "peer_handshake.h"

#include <errno.h>
#include <netinet/in.h>
#include <algo/algo.h>

using namespace algo;

namespace torrent {

PeerHandshake::Handshakes PeerHandshake::m_handshakes;

// Incoming connections.
PeerHandshake::PeerHandshake(int fdesc, const std::string dns, unsigned short port)  :
  m_fd(fdesc),
  m_peer("", dns, port),
  m_download(NULL),
  m_state(READ_HEADER),
  m_incoming(true),
  m_buf(new char[256+48]),
  m_pos(0)
{
  insertRead();
  insertExcept();
}

// Outgoing connections.
PeerHandshake::PeerHandshake(int fdesc, const Peer& p, DownloadState* d) :
  m_fd(fdesc),
  m_peer(p),
  m_peerOrig(p),
  m_download(d),
  m_state(CONNECTING),
  m_incoming(false),
  m_buf(new char[256+48]),
  m_pos(0)
{
  insertWrite();
  insertExcept();

  m_buf[0] = 19;
  std::memcpy(&m_buf[1], "BitTorrent protocol", 19);
  std::memset(&m_buf[20], 0, 8);
  std::memcpy(&m_buf[28], m_download->hash().c_str(), 20);
  std::memcpy(&m_buf[48], m_download->me().id().c_str(), 20);
}

PeerHandshake::~PeerHandshake() {
  removeConnection(this);

  delete [] m_buf;

  if (m_fd >= 0)
    ::close(m_fd);
}

void PeerHandshake::connect(int fdesc, const std::string dns, unsigned short port) {
  if (fdesc < 0)
    throw internal_error("Tried to assign negative file desc to PeerHandshake");

  set_socket_async(fdesc);

  // TODO: add checks so we don't do multiple connections.
  addConnection(new PeerHandshake(fdesc, dns, port));
}

bool PeerHandshake::connect(const Peer& p, DownloadState* d) {
  if (!p.is_valid())
    throw internal_error("Tried to connect with invalid peer information");

  try {
    
    sockaddr_in sa;
    make_sockaddr(p.dns(), p.port(), sa);

    addConnection(new PeerHandshake(make_socket(sa), p, d));

    return true;

  } catch (network_error& e) {
    return false;
  }
}

void PeerHandshake::read() {
  Download* d;

  try {

  switch (m_state) {
  case READ_HEADER:
    if (!recv1())
      return;

    if (m_incoming) {
      if ((d = Download::getDownload(m_infoHash)) != NULL) {
	m_download = &d->state();

	removeRead();
	insertWrite();
	
	m_buf[0] = 19;
	std::memcpy(&m_buf[1], "BitTorrent protocol", 19);
	std::memset(&m_buf[20], 0, 8);
	std::memcpy(&m_buf[28], m_download->hash().c_str(), 20);
	std::memcpy(&m_buf[48], m_download->me().id().c_str(), 20);
	
	m_state = WRITE_HEADER;
	m_pos = 0;
	
	return;

      } else {
	throw communication_error("Peer connection for unknown file hash");
      }

    } else {
      if (m_infoHash != m_download->hash())
	throw communication_error("Peer returned bad file hash");

      m_state = READ_ID;
      m_pos = 0;
    }

  case READ_ID:
    if (!recv2())
      return;

    if (m_incoming &&
	m_peer.id() == m_download->me().id())
      throw communication_error("Connected to client with the same id");
      
    else if (!m_incoming &&
	     (m_peer.id() != m_peerOrig.id() ||
	      m_peer.id() == m_download->me().id()))
      throw communication_error("Peer returned bad peer id");

    m_download->addConnection(m_fd, m_peer);
    m_fd = -1;
      
    delete this;
    return;

  default:
    throw internal_error("PeerHandshake::read() called on object in wrong state");
  }

  } catch (network_error& e) {
    delete this;
  }
}

void PeerHandshake::write() {
  int error;

  try {

  switch (m_state) {
  case CONNECTING:
    error = get_socket_error(m_fd);

    if (error != 0) {
      throw network_error("Could not connect to client");
    }

    m_state = WRITE_HEADER;

  case WRITE_HEADER:
    if (!writeBuf(m_buf + m_pos, 68, m_pos))
      return;

    removeWrite();
    insertRead();

    m_pos = 0;
    m_state = m_incoming ? READ_ID : READ_HEADER;

    return;

  default:
    throw internal_error("PeerHandshake::write() called on object in wrong state");
  }

  } catch (network_error& e) {
    delete this;
  }
}

void PeerHandshake::except() {
  delete this;
}

int PeerHandshake::fd() {
  return m_fd;
}

bool PeerHandshake::recv1() {
  if (m_pos == 0 &&
      !readBuf(m_buf, 1, m_pos))
    return false;

  int l = (unsigned char)m_buf[0];

  if (!readBuf(m_buf + m_pos, l + 29, m_pos))
    return false;

  m_peer.ref_protocol() = std::string(&m_buf[1], l);
  m_peer.ref_options()  = std::string(&m_buf[1+l], 8);
  m_infoHash            = std::string(&m_buf[9+l], 20);

  if (m_peer.protocol() != "BitTorrent protocol") {
    throw communication_error("Peer returned wrong protocol identifier");
  } else {
    return true;
  }
}

bool PeerHandshake::recv2() {
  if (!readBuf(m_buf + m_pos, 20, m_pos))
    return false;

  m_peer.ref_id() = std::string(m_buf, 20);

  return true;
}  

void PeerHandshake::addConnection(PeerHandshake* p) {
  if (p == NULL)
    throw internal_error("Tried to add bad PeerHandshake to handshake cue");

  if (m_handshakes.size() > 200)
    throw internal_error("Handshake queue is bigger than 200");

  m_handshakes.push_back(p);
}

void PeerHandshake::removeConnection(PeerHandshake* p) {
  Handshakes::iterator itr = std::find(m_handshakes.begin(), m_handshakes.end(), p);

  if (itr == m_handshakes.end())
    throw internal_error("Could not remove connection from torrent, not found");

  m_handshakes.erase(itr);
}

bool PeerHandshake::isConnecting(const std::string& id) {
  return std::find_if(m_handshakes.begin(), m_handshakes.end(),
		      eq(ref(id), on<const std::string&>(call_member(&PeerHandshake::peer),
							 call_member(&Peer::id))))
    != m_handshakes.end();
}

}
