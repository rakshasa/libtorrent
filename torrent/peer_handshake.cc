#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "general.h"
#include "exceptions.h"
#include "download.h"
#include "peer_handshake.h"
#include "tracker_query.h"

#include <errno.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
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
PeerHandshake::PeerHandshake(int fdesc, const Peer& p, Download* d, bool connected) :
  m_fd(fdesc),
  m_peer(p),
  m_peerOrig(p),
  m_download(d),
  m_state(connected ? WRITE_HEADER : CONNECTING),
  m_incoming(false),
  m_buf(new char[256+48]),
  m_pos(0)
{
  setSocketAsync(m_fd);

  insertWrite();
  insertExcept();

  m_buf[0] = 19;
  std::memcpy(&m_buf[1], "BitTorrent protocol", 19);
  std::memset(&m_buf[20], 0, 8);
  std::memcpy(&m_buf[28], m_download->tracker().hash().c_str(), 20);
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

  setSocketAsync(fdesc);

  // TODO: add checks so we don't do multiple connections.
  addConnection(new PeerHandshake(fdesc, dns, port));
}

bool PeerHandshake::connect(const Peer& p, Download* d) {
  if (!p.valid())
    throw internal_error("Tried to connect with invalid peer information");

  hostent* he = gethostbyname(p.dns().c_str());

  if (he == NULL) {
    return false;
  }

  int fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

  if (fd < 0)
    throw local_error("Could not allocate socket for connecting");

  setSocketAsync(fd);

  sockaddr_in sa;
  std::memset(&sa, 0, sizeof(sockaddr_in));

  sa.sin_family = AF_INET;
  sa.sin_port = htons(p.port());

  std::memcpy(&sa.sin_addr, he->h_addr_list[0], sizeof(in_addr));

  if (::connect(fd, (sockaddr*)&sa, sizeof(sockaddr_in)) == 0) {
    addConnection(new PeerHandshake(fd, p, d, true));

  } else if (errno == EINPROGRESS) {
    addConnection(new PeerHandshake(fd, p, d, false));

  } else {
    ::close(fd);
    return false;
  }

  return true;
}

void PeerHandshake::read() {
  try {

  switch (m_state) {
  case READ_HEADER:
    if (!recv1())
      return;

    if (m_incoming) {
      if ((m_download = Download::getDownload(m_infoHash)) != NULL) {
	removeRead();
	insertWrite();
	
	m_buf[0] = 19;
	std::memcpy(&m_buf[1], "BitTorrent protocol", 19);
	std::memset(&m_buf[20], 0, 8);
	std::memcpy(&m_buf[28], m_download->tracker().hash().c_str(), 20);
	std::memcpy(&m_buf[48], m_download->me().id().c_str(), 20);
	
	m_state = WRITE_HEADER;
	m_pos = 0;
	
	return;

      } else {
	throw communication_error("Peer connection for unknown file hash");
      }

    } else {
      if (m_infoHash != m_download->tracker().hash())
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
    error = getSocketError(m_fd);

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

  m_peer.refProtocol() = std::string(&m_buf[1], l);
  m_peer.refOptions()  = std::string(&m_buf[1+l], 8);
  m_infoHash           = std::string(&m_buf[9+l], 20);

  if (m_peer.protocol() != "BitTorrent protocol") {
    throw communication_error("Peer returned wrong protocol identifier");
  } else {
    return true;
  }
}

bool PeerHandshake::recv2() {
  if (!readBuf(m_buf + m_pos, 20, m_pos))
    return false;

  m_peer.refId() = std::string(m_buf, 20);

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
