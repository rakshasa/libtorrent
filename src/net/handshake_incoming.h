#ifndef LIBTORRENT_HANDSHAKE_INCOMING_H
#define LIBTORRENT_HANDSHAKE_INCOMING_H

#include "handshake.h"

namespace torrent {

class HandshakeIncoming : public Handshake {
public:
  typedef enum {
    INACTIVE,
    WRITE_HEADER,
    READ_HEADER1,
    READ_HEADER2
  } State;

  HandshakeIncoming(int fd, HandshakeManager* m, const PeerInfo& p);
  
  virtual void        read();
  virtual void        write();
  virtual void        except();

private:
  State               m_state;
};

}

#endif
