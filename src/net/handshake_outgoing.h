#ifndef LIBTORRENT_HANDSHAKE_OUTGOING_H
#define LIBTORRENT_HANDSHAKE_OUTGOING_H

#include "handshake.h"

namespace torrent {

class HandshakeOutgoing : public Handshake {
public:
  typedef enum {
    INACTIVE,
    CONNECTING,
    WRITE_HEADER,
    READ_HEADER1,
    READ_HEADER2
  } State;

  HandshakeOutgoing(int fd,
		    HandshakeManager* m,
		    const PeerInfo& p,
		    const std::string& infoHash,
		    const std::string& ourId);
  
  virtual void        read();
  virtual void        write();
  virtual void        except();

private:
  State               m_state;
  std::string         m_local;
};

}

#endif
