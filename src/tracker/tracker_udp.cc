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
#include <stdlib.h>
#include <sstream>
#include <sigc++/bind.h>

#include "net/manager.h"
#include "torrent/exceptions.h"

#include "tracker_udp.h"

namespace torrent {

std::string
_string_to_hex(const std::string& src) {
  std::stringstream stream;

  stream << std::hex << std::uppercase;

  for (std::string::const_iterator itr = src.begin(); itr != src.end(); ++itr)
    stream << ((unsigned char)*itr >> 4) << ((unsigned char)*itr & 0xf);

  return stream.str();
}

TrackerUdp::TrackerUdp(TrackerInfo* info, const std::string& url) :
  TrackerBase(info, url),
  m_readBuffer(NULL),
  m_writeBuffer(NULL) {

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

  if (!m_fd.open_datagram() ||
      !m_fd.set_nonblock() ||
      //(!m_bindAddress.is_any() && !m_fd.bind(m_bindAddress)))
      !m_fd.bind(m_bindAddress))
    return receive_failed("Could not open UDP socket");

  m_readBuffer = new ReadBuffer;
  m_writeBuffer = new WriteBuffer;

  prepare_connect_input();

  pollManager.write_set().insert(this);
  pollManager.except_set().insert(this);
}

void
TrackerUdp::close() {
  if (!m_fd.is_valid())
    return;

  delete m_readBuffer;
  delete m_writeBuffer;

  m_readBuffer = NULL;
  m_writeBuffer = NULL;

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

  int s = receive(m_readBuffer->begin(), m_readBuffer->reserved(), &sa);

  if (s < 0)
    m_slotLog("UDP read() got error " + std::string(std::strerror(get_errno())));
  else if (s > 0)
    m_slotLog("UDP read() got message from " + sa.get_address());
  else
    m_slotLog("UDP read() got zero");

  m_readBuffer->reset_position();
  m_readBuffer->set_end(s);

  //m_connectAddress = sa;
  //pollManager.write_set().insert(this);
}

void
TrackerUdp::write() {
  if (m_writeBuffer->size_end() == 0)
    throw internal_error("TrackerUdp::write() called but the write buffer is empty.");

  int s = send(m_writeBuffer->begin(), m_writeBuffer->size_end(), &m_connectAddress);

  if (s != m_writeBuffer->size_end())
    m_slotLog("UDP write failed");
  else
    m_slotLog("UDP write \"" + _string_to_hex(std::string((char*)m_writeBuffer->begin(), m_writeBuffer->size_end())));

  m_writeBuffer->prepare_end();
  pollManager.write_set().erase(this);
  pollManager.read_set().insert(this); // Propably insert into read set immidiately.
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

void
TrackerUdp::prepare_connect_input() {
  // Fill structure.
  m_writeBuffer->reset_position();
  m_writeBuffer->write64(m_connectionId = magic_connection_id);
  m_writeBuffer->write32(m_action = 0);
  m_writeBuffer->write32(m_transactionId = random());

  m_writeBuffer->prepare_end();
}

}
