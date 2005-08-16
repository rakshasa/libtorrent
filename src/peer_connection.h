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
// In addition, as a special exception, the copyright holders give
// permission to link the code of portions of this program with the
// OpenSSL library under certain conditions as described in each
// individual source file, and distribute linked combinations
// including the two.
//
// You must obey the GNU General Public License in all respects for
// all of the code used other than OpenSSL.  If you modify file(s)
// with this exception, you may extend this exception to your version
// of the file(s), but you are not obligated to do so.  If you do not
// wish to do so, delete this exception statement from your version.
// If you delete this exception statement from all source files in the
// program, then also delete it here.
//
// Contact:  Jari Sundell <jaris@ifi.uio.no>
//
//           Skomakerveien 33
//           3185 Skoppum, NORWAY

#ifndef LIBTORRENT_PEER_CONNECTION_H
#define LIBTORRENT_PEER_CONNECTION_H

#include "protocol/peer_connection_base.h"
#include "utils/throttle.h"

#include <vector>

// Yes, this source is a horrible mess. But its stable enough so i won't
// start refactoring until i finish the other stuff i need to do.

namespace torrent {

class PeerConnection : public PeerConnectionBase {
public:
  PeerConnection();
  virtual ~PeerConnection();

  inline bool         is_choke_delayed();

  virtual void        set_choke(bool v);

  virtual void        update_interested();

  virtual void        receive_have_chunk(int32_t i);
  virtual bool        receive_keepalive();

  virtual void        event_read();
  virtual void        event_write();
  virtual void        event_error();

  void set(SocketFd fd, const PeerInfo& p, DownloadMain* download);
  
private:
  PeerConnection(const PeerConnection&);
  PeerConnection& operator = (const PeerConnection&);

  // Parse packet in read buffer, must be of correct type.
  void parseReadBuf();

  void fillWriteBuf();

  bool                send_request_piece();

  void                receive_request_piece(Piece p);
  void                receive_cancel_piece(Piece p);
  void                receive_have(uint32_t index);

  void                receive_piece_header(Piece p);

  void                task_send_choke();
  void                task_stall();

  bool           m_shutdown;

  int            m_stallCount;

  bool           m_sendChoked;
  bool           m_sendInterested;
  bool           m_tryRequest;

  std::list<int> m_haveQueue;

  Timer          m_lastMsg;

  TaskItem            m_taskSendChoke;
  TaskItem            m_taskStall;
};

inline bool
PeerConnection::is_choke_delayed() {
  return m_sendChoked || taskScheduler.is_scheduled(&m_taskSendChoke);
}

}

#endif // LIBTORRENT_PEER_CONNECTION_H
