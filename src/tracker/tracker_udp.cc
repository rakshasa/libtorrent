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

#include <stdlib.h>
#include <rak/error_number.h>

#include "net/manager.h"
#include "torrent/exceptions.h"

#include "tracker_udp.h"

namespace torrent {

TrackerUdp::TrackerUdp(TrackerInfo* info, const std::string& url) :
  TrackerBase(info, url),
  m_readBuffer(NULL),
  m_writeBuffer(NULL) {

  m_taskTimeout.set_slot(rak::mem_fn(this, &TrackerUdp::receive_timeout));
}

TrackerUdp::~TrackerUdp() {
  close();
}
  
bool
TrackerUdp::is_busy() const {
  return get_fd().is_valid();
}

void
TrackerUdp::send_state(TrackerInfo::State state,
		       uint64_t down,
		       uint64_t up,
		       uint64_t left) {
  close();

  if (!parse_url())
    return receive_failed("Could not parse UDP hostname or port");

  if (!get_fd().open_datagram() ||
      !get_fd().set_nonblock() ||
      !get_fd().bind(m_bindAddress))
    return receive_failed("Could not open UDP socket");

  m_readBuffer = new ReadBuffer;
  m_writeBuffer = new WriteBuffer;

  m_sendState = state;
  m_sendDown = down;
  m_sendUp = up;
  m_sendLeft = left;

  prepare_connect_input();

  pollCustom->open(this);
  pollCustom->insert_read(this);
  pollCustom->insert_write(this);
  pollCustom->insert_error(this);

  m_tries = m_info->udp_tries();
  taskScheduler.push(m_taskTimeout.prepare((cachedTime + m_info->udp_timeout() * 1000000).round_seconds()));
}

void
TrackerUdp::close() {
  if (!get_fd().is_valid())
    return;

  delete m_readBuffer;
  delete m_writeBuffer;

  m_readBuffer = NULL;
  m_writeBuffer = NULL;

  taskScheduler.erase(&m_taskTimeout);

  pollCustom->remove_read(this);
  pollCustom->remove_write(this);
  pollCustom->remove_error(this);
  pollCustom->close(this);

  get_fd().close();
  get_fd().clear();
}

TrackerUdp::Type
TrackerUdp::get_type() const {
  return TRACKER_UDP;
}

void
TrackerUdp::receive_failed(const std::string& msg) {
  close();
  m_slotFailed(this, msg);
}

void
TrackerUdp::receive_timeout() {
  if (m_taskTimeout.is_queued())
    throw internal_error("TrackerUdp::receive_timeout() called but m_taskTimeout is still scheduled.");

  if (--m_tries == 0) {
    receive_failed("Unable to connect to UDP tracker.");
  } else {
    taskScheduler.push(m_taskTimeout.prepare((cachedTime + m_info->udp_timeout() * 1000000).round_seconds()));

    pollCustom->insert_write(this);
  }
}

void
TrackerUdp::event_read() {
  SocketAddress sa;

  int s = read_datagram(m_readBuffer->begin(), m_readBuffer->reserved(), &sa);

  if (s < 4)
    return;

  m_readBuffer->reset_position();
  m_readBuffer->set_end(s);

  // Make sure sa is from the source we expected?

  // Do something with the content here.
  switch (m_readBuffer->read_32()) {
  case 0:
    if (m_action != 0 || !process_connect_output())
      return;

    prepare_announce_input();

    taskScheduler.erase(&m_taskTimeout);
    taskScheduler.push(m_taskTimeout.prepare((cachedTime + m_info->udp_timeout() * 1000000).round_seconds()));

    m_tries = m_info->udp_tries();
    pollCustom->insert_write(this);
    return;

  case 1:
    if (m_action != 1 || !process_announce_output())
      return;

    return;

  case 3:
    if (!process_error_output())
      return;

    return;

  default:
    return;
  };
}

void
TrackerUdp::event_write() {
  if (m_writeBuffer->size_end() == 0)
    throw internal_error("TrackerUdp::write() called but the write buffer is empty.");

  int s = write_datagram(m_writeBuffer->begin(), m_writeBuffer->size_end(), &m_connectAddress);

  // TODO: If send failed, retry shortly or do i call receive_failed?
  if (s != m_writeBuffer->size_end())
    ;

  pollCustom->remove_write(this);
}

void
TrackerUdp::event_error() {
}

bool
TrackerUdp::parse_url() {
  int port;
  char hostname[256];
      
  if (std::sscanf(m_url.c_str(), "udp://%255[^:]:%i", hostname, &port) != 2 ||
      hostname[0] == '\0' ||
      port <= 0 || port >= (1 << 16))
    return false;

  m_connectAddress.set_hostname(hostname);
  m_connectAddress.set_port(port);

  return !m_connectAddress.is_port_any() && !m_connectAddress.is_address_any();
}

void
TrackerUdp::prepare_connect_input() {
  m_writeBuffer->reset_position();
  m_writeBuffer->write_64(m_connectionId = magic_connection_id);
  m_writeBuffer->write_32(m_action = 0);
  m_writeBuffer->write_32(m_transactionId = random());

  m_writeBuffer->prepare_end();
}

void
TrackerUdp::prepare_announce_input() {
  m_writeBuffer->reset_position();

  m_writeBuffer->write_64(m_connectionId);
  m_writeBuffer->write_32(m_action = 1);
  m_writeBuffer->write_32(m_transactionId = random());

  m_writeBuffer->write_range(m_info->get_hash().begin(), m_info->get_hash().end());
  m_writeBuffer->write_range(m_info->get_local_id().begin(), m_info->get_local_id().end());

  m_writeBuffer->write_64(m_sendDown);
  m_writeBuffer->write_64(m_sendLeft);
  m_writeBuffer->write_64(m_sendUp);
  m_writeBuffer->write_32(m_sendState);

  m_writeBuffer->write_32(m_info->get_local_address().get_addr_in_addr());
  m_writeBuffer->write_32(m_info->get_key());
  m_writeBuffer->write_32(m_info->get_numwant());
  m_writeBuffer->write_16(m_info->get_local_address().get_port());

  m_writeBuffer->prepare_end();

  if (m_writeBuffer->size_end() != 98)
    throw internal_error("TrackerUdp::prepare_announce_input() ended up with the wrong size");
}

bool
TrackerUdp::process_connect_output() {
  if (m_readBuffer->size_end() < 16 ||
      m_readBuffer->read_32() != m_transactionId)
    return false;

  m_connectionId = m_readBuffer->read_64();

  return true;
}

bool
TrackerUdp::process_announce_output() {
  if (m_readBuffer->size_end() < 20 ||
      m_readBuffer->read_32() != m_transactionId)
    return false;

  m_slotSetInterval(m_readBuffer->read_32());

  m_readBuffer->read_32(); // leechers
  m_readBuffer->read_32(); // seeders

  AddressList l;

  std::copy(reinterpret_cast<const SocketAddressCompact*>(m_readBuffer->position()),
	    reinterpret_cast<const SocketAddressCompact*>(m_readBuffer->end() - m_readBuffer->remaining() % sizeof(SocketAddressCompact)),
	    std::back_inserter(l));

  // Some logic here to decided on whetever we're going to close the
  // connection or not?
  close();
  m_slotSuccess(this, &l);
  return true;
}
  
bool
TrackerUdp::process_error_output() {
  if (m_readBuffer->size_end() < 8 ||
      m_readBuffer->read_32() != m_transactionId)
    return false;

  receive_failed("Received error message: " + std::string(m_readBuffer->position(), m_readBuffer->end()));
  return true;
}

}
