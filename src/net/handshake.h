#ifndef LIBTORRENT_PEER_HANDSHAKE_H
#define LIBTORRENT_PEER_HANDSHAKE_H

#include "peer_info.h"

#include "net/socket_base.h"

#include <sstream>
#include <list>

namespace torrent {

class DownloadState;

// This class handles the initial handshake and then hands
// the socket to the downloader.
class Handshake : SocketBase {
public:
  typedef std::list<Handshake*> Handshakes;

  typedef enum {
    INACTIVE,      // We can't return to being inactive, destroy when closing.
    CONNECTING,
    READ_HEADER,
    WRITE_HEADER,
    READ_ID
  } State;

  ~Handshake();

  static void connect(int fdesc, const std::string dns, unsigned short port);
  static bool connect(const PeerInfo& p, DownloadState* d);

  // Only checks for outgoing connections.
  static bool isConnecting(const std::string& id);

  virtual void read();
  virtual void write();
  virtual void except();
   
  const PeerInfo& peer() const { return m_peer; }
  DownloadState* download() const { return m_download; }

  static Handshakes& handshakes() { return m_handshakes; }

protected: // Disable private ctor only warning.
  Handshake(int fdesc, const std::string dns, unsigned short port);
  Handshake(int fdesc, const PeerInfo& p, DownloadState* d);

private:
  // Disable
  Handshake();
  Handshake(const Handshake& p);
  Handshake& operator = (const Handshake& p);

  void close();

  bool recv1();
  bool recv2();

  static void Handshake::addConnection(Handshake* p);
  static void Handshake::removeConnection(Handshake* p);

  static Handshakes m_handshakes;

  std::string m_infoHash;

  PeerInfo m_peer;
  DownloadState* m_download;

  // internal state
  State m_state;
  bool m_incoming;

  char* m_buf;
  unsigned int m_pos;
};

} // namespace torrent

#endif // LIBTORRENT_PEER_HANDSHAKE_H
