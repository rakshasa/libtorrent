#ifndef LIBTORRENT_PEER_CONNECTION_H
#define LIBTORRENT_PEER_CONNECTION_H

#include "bitfield.h"
#include "data/storage.h"
#include "socket_base.h"
#include "service.h"
#include "peer_info.h"
#include "piece.h"
#include "rate.h"
#include "throttle.h"

#include <vector>

namespace torrent {

class DownloadState;

class PeerConnection : public SocketBase, public Service {
public:
  typedef enum {
    IDLE = 1,
    READ_LENGTH,
    READ_TYPE,
    READ_MSG,
    READ_BITFIELD,
    READ_PIECE,
    READ_SKIP_PIECE,
    WRITE_MSG,
    WRITE_BITFIELD,
    WRITE_PIECE,
    SHUTDOWN
  } State;

  typedef enum {
    CHOKE = 0,
    UNCHOKE,
    INTERESTED,
    NOT_INTERESTED,
    HAVE,
    BITFIELD,
    REQUEST,
    PIECE,
    CANCEL,
    NONE,           // These are not part of the protocol
    KEEP_ALIVE      // Last command was a keep alive
  } Protocol;

  typedef std::list<Piece> PieceList;

#include "peer_connection_sub.h"

  PeerConnection();
  ~PeerConnection();

  void set(int fd, const PeerInfo& p, DownloadState* d);
  
  void service(int type);

  void sendHave(int i);
  void choke(bool v);

  bool chokeDelayed();
  Timer lastChoked() { return m_lastChoked; }

  const BitField& bitfield() const { return m_bitfield; }

  const PeerInfo& peer() const { return m_peer; }

  Sub& up() { return m_up; }
  Sub& down() { return m_down; }

  Throttle& throttle() { return m_throttle; }

  virtual void read();
  virtual void write();
  virtual void except();
   
  virtual int fd();

private:
  PeerConnection(const PeerConnection&);
  PeerConnection& operator = (const PeerConnection&);

  enum {
    SERVICE_KEEP_ALIVE = 0x2000,
    SERVICE_SEND_CHOKE,
    SERVICE_INCOMING_PIECE
  };

  bool writeChunk(int maxBytes);
  bool readChunk();

  void discardIncomingQueue();

  // Send a msg to the buffer.
  void bufCmd(Protocol cmd, unsigned int length, unsigned int send = 0);
  void bufW32(uint32_t v);
  uint32_t bufR32(bool peep = false);

  // Parse packet in read buffer, must be of correct type.
  void parseReadBuf();

  void fillWriteBuf();

  int m_fd;
  bool m_shutdown;

  PeerInfo m_peer;
  DownloadState* m_download;
   
  BitField m_bitfield;
   
  bool m_sendChoked;
  bool m_sendInterested;

  std::list<int> m_haveQueue;

  Timer m_lastChoked;
  Timer m_lastMsg;

  Sub m_up;
  Sub m_down;

  Throttle m_throttle;
};

}

#endif // LIBTORRENT_PEER_CONNECTION_H
