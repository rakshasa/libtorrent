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

#include "config.h"

#include <cerrno>
#include <sigc++/bind.h>

#include "net/manager.h"
#include "torrent/exceptions.h"

#include "tracker_udp.h"

namespace torrent {

TrackerUdp::TrackerUdp(TrackerInfo* info, const std::string& url) :
  TrackerBase(info, url) {

  m_taskDelay.set_iterator(taskScheduler.end());
}

TrackerUdp::~TrackerUdp() {
  taskScheduler.erase(&m_taskDelay);
  close();
}
  
bool
TrackerUdp::is_busy() const {
  return m_fd.is_valid();
}

void
TrackerUdp::send_state(TrackerInfo::State state,
		       uint64_t down,
		       uint64_t up,
		       uint64_t left) {
  close();

  return receive_failed("UDP tracker support not yet implemented");

  if (!parse_url())
    return receive_failed("Could not parse UDP hostname or port");

  // Debug
  if (m_connectAddress.get_port() % 2)
    m_bindAddress.set_port(m_connectAddress.get_port() + 1);
  else
    //m_bindAddress.set_port(m_connectAddress.get_port() - 1);
    //connectAddress = SocketAddress();
    ;

  if (!m_fd.open_datagram() ||
      !m_fd.set_nonblock() ||
      (!m_bindAddress.is_any() && !m_fd.bind(m_bindAddress))// ||
      //!m_fd.bind(m_bindAddress) ||
      //!m_fd.connect(connectAddress)
      )
    return receive_failed("Could not open UDP socket");

  pollManager.read_set().insert(this);
  pollManager.write_set().insert(this);
  pollManager.except_set().insert(this);

  m_readBuffer = new char[512];
  m_writeBuffer = new char[512];
  
}

void
TrackerUdp::close() {
  if (!m_fd.is_valid())
    return;

  delete m_readBuffer;
  delete m_writeBuffer;

  pollManager.read_set().erase(this);
  pollManager.write_set().erase(this);
  pollManager.except_set().erase(this);

  m_fd.close();
  m_fd.clear();
}

void
TrackerUdp::receive_failed(const std::string& msg) {
  close();
  m_slotFailed(msg);
}

void
TrackerUdp::read() {
  SocketAddress sa;

  int m_readLength = receive(m_readBuffer, 512, &sa);

  if (m_readLength < 0)
    m_slotLog("UDP read() got error " + std::string(std::strerror(get_errno())));
  else if (m_readLength > 0)
    m_slotLog("UDP read() got message from " + sa.get_address());

  m_connectAddress = sa;
  pollManager.write_set().insert(this);
}

void
TrackerUdp::write() {
  pollManager.write_set().erase(this);

  m_writeBuffer[0] = 1;
  
  int s = send(m_writeBuffer, 1, &m_connectAddress);

  if (s != 1)
    m_slotLog("UDP send failed");
}

void
TrackerUdp::except() {
  m_slotLog("UDP except() called");
}

bool
TrackerUdp::parse_url() {
  int port;
  char hostname[256];
      
  if (std::sscanf(m_url.c_str(), "udp://%256[^:]:%i", hostname, &port) != 2 ||
      hostname[0] == '\0' ||
      port <= 0 || port >= (1 << 16))
    return false;

  m_connectAddress.set_hostname(hostname);
  m_connectAddress.set_port(port);

  return !m_connectAddress.is_port_any() && !m_connectAddress.is_address_any();
}

}
