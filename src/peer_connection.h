// libTorrent - BitTorrent library
// Copyright (C) 2005, Jari Sundell
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
// Contact:  Jari Sundell <jaris@ifi.uio.no>
//
//           Skomakerveien 33
//           3185 Skoppum, NORWAY

#ifndef LIBTORRENT_PEER_CONNECTION_H
#define LIBTORRENT_PEER_CONNECTION_H

#include "peer_info.h"
#include "data/piece.h"
#include "utils/rate.h"
#include "throttle.h"

#include "data/storage.h"
#include "peer/request_list.h"
#include "net/socket_base.h"
#include "utils/bitfield_ext.h"
#include "utils/task.h"

#include <vector>

// Yes, this source is a horrible mess. But its stable enough so i won't
// start refactoring until i finish the other stuff i need to do.

namespace torrent {

class DownloadState;
class DownloadNet;

class PeerConnection : public SocketBase {
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

  typedef std::list<Piece>              SendList;

#include "peer_connection_sub.h"

  PeerConnection();
  ~PeerConnection();

  void sendHave(int i);
  void choke(bool v);

  void update_interested();

  bool chokeDelayed();
  Timer lastChoked() { return m_lastChoked; }

  const BitFieldExt& bitfield() const { return m_bitfield; }

  const PeerInfo& peer() const { return m_peer; }

  Sub& up() { return m_up; }
  Sub& down() { return m_down; }

  RequestList& get_requests() { return m_requests; }
  SendList&    get_sends()    { return m_sends; }

  Rate&        get_rate_peer() { return m_ratePeer; }

  Throttle& throttle() { return m_throttle; }

  bool is_up_choked() { return m_up.c_choked(); }

  virtual void read();
  virtual void write();
  virtual void except();

  static PeerConnection* create(int fd, const PeerInfo& p, DownloadState* d, DownloadNet* net);

private:
  PeerConnection(const PeerConnection&);
  PeerConnection& operator = (const PeerConnection&);

  void set(int fd, const PeerInfo& p, DownloadState* d, DownloadNet* net);
  
  bool writeChunk(int maxBytes);
  bool readChunk();

  void load_chunk(int index, Sub& sub);

  bool request_piece();
  void skip_piece();

  // Send a msg to the buffer.
  void bufCmd(Protocol cmd, unsigned int length, unsigned int send = 0);
  void bufW32(uint32_t v);
  uint32_t bufR32(bool peep = false);

  // Parse packet in read buffer, must be of correct type.
  void parseReadBuf();

  void fillWriteBuf();

  void task_keep_alive();
  void task_send_choke();
  void task_stall();

  bool           m_shutdown;

  int            m_stallCount;

  PeerInfo       m_peer;
  DownloadState* m_download;
  DownloadNet*   m_net;
   
  BitFieldExt    m_bitfield;
   
  bool           m_sendChoked;
  bool           m_sendInterested;
  bool           m_tryRequest;

  SendList       m_sends;
  RequestList    m_requests;

  std::list<int> m_haveQueue;

  Timer          m_lastChoked;
  Timer          m_lastMsg;

  Sub            m_up;
  Sub            m_down;

  Rate           m_ratePeer;
  Throttle       m_throttle;

  Task           m_taskKeepAlive;
  Task           m_taskSendChoke;
  Task           m_taskStall;
};

}

#endif // LIBTORRENT_PEER_CONNECTION_H
