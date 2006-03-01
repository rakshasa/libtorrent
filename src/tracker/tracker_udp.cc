// libTorrent - BitTorrent library
// Copyright (C) 2005-2006, Jari Sundell
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

#include <rak/address_info.h>
#include <rak/error_number.h>

#include "net/manager.h"
#include "torrent/exceptions.h"

#include "tracker_udp.h"

namespace torrent {

TrackerUdp::TrackerUdp(DownloadInfo* info, const std::string& url) :
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
TrackerUdp::send_state(DownloadInfo::State state, uint64_t down, uint64_t up, uint64_t left) {
  close();

  if (!parse_url())
    return receive_failed("Could not parse UDP hostname or port.");

  if (!get_fd().open_datagram() ||
      !get_fd().set_nonblock() ||
      !get_fd().bind(*socketManager.bind_address()))
    return receive_failed("Could not open UDP socket.");

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
  priority_queue_insert(&taskScheduler, &m_taskTimeout, (cachedTime + rak::timer::from_seconds(m_info->udp_timeout())).round_seconds());
}

void
TrackerUdp::close() {
  if (!get_fd().is_valid())
    return;

  delete m_readBuffer;
  delete m_writeBuffer;

  m_readBuffer = NULL;
  m_writeBuffer = NULL;

  priority_queue_erase(&taskScheduler, &m_taskTimeout);

  pollCustom->remove_read(this);
  pollCustom->remove_write(this);
  pollCustom->remove_error(this);
  pollCustom->close(this);

  get_fd().close();
  get_fd().clear();
}

TrackerUdp::Type
TrackerUdp::type() const {
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
    priority_queue_insert(&taskScheduler, &m_taskTimeout, (cachedTime + rak::timer::from_seconds(m_info->udp_timeout())).round_seconds());

    pollCustom->insert_write(this);
  }
}

void
TrackerUdp::event_read() {
  rak::socket_address sa;

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

    priority_queue_erase(&taskScheduler, &m_taskTimeout);
    priority_queue_insert(&taskScheduler, &m_taskTimeout, (cachedTime + rak::timer::from_seconds(m_info->udp_timeout())).round_seconds());

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
  char hostname[1024];
      
  if (std::sscanf(m_url.c_str(), "udp://%1023[^:]:%i", hostname, &port) != 2 ||
      hostname[0] == '\0' ||
      port <= 0 || port >= (1 << 16))
    return false;

  int err;
  rak::address_info* ai;

  if ((err = rak::address_info::get_address_info(hostname, PF_INET, SOCK_STREAM, &ai)) != 0)
    return false;
  
  if (ai->address()->family() != rak::socket_address::af_inet)
    return false;

  m_connectAddress.copy(*ai->address(), ai->length());
  m_connectAddress.set_port(port);

  return m_connectAddress.is_valid();
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

  m_writeBuffer->write_range(m_info->hash().begin(), m_info->hash().end());
  m_writeBuffer->write_range(m_info->local_id().begin(), m_info->local_id().end());

  m_writeBuffer->write_64(m_sendDown);
  m_writeBuffer->write_64(m_sendLeft);
  m_writeBuffer->write_64(m_sendUp);
  m_writeBuffer->write_32(m_sendState);

  // This code assumes we're have a inet address.
  if (m_info->local_address()->family() != rak::socket_address::af_inet)
    throw internal_error("TrackerUdp::prepare_announce_input() m_info->local_address() not of family AF_INET.");

  m_writeBuffer->write_32_n(m_info->local_address()->sa_inet()->address_n());
  m_writeBuffer->write_32(m_info->key());
  m_writeBuffer->write_32(m_info->numwant());
  m_writeBuffer->write_16(m_info->port());

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
