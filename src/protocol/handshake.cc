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

#include "download/download_info.h"
#include "net/manager.h"
#include "torrent/exceptions.h"

#include "globals.h"

#include "handshake.h"
#include "handshake_manager.h"

namespace torrent {

const char* Handshake::m_protocol = "BitTorrent protocol";

Handshake::Handshake(SocketFd fd, HandshakeManager* m) :
  m_state(INACTIVE),

  m_manager(m),
  m_downloadInfo(NULL) {

  set_fd(fd);

  m_readBuffer.reset();
  m_writeBuffer.reset();      

  m_taskTimeout.set_slot(rak::bind_mem_fn(m, &HandshakeManager::receive_failed, this));
}

Handshake::~Handshake() {
  if (m_taskTimeout.is_queued())
    throw internal_error("Handshake m_taskTimeout bork bork bork.");

  if (get_fd().is_valid())
    throw internal_error("Handshake dtor called but m_fd is still open");
}

void
Handshake::initialize_outgoing(const rak::socket_address& sa, DownloadInfo* d) {
  m_downloadInfo = d;

  m_peerInfo.set_incoming(false);
  m_peerInfo.set_socket_address(&sa);

  m_state = CONNECTING;

  pollCustom->open(this);
  pollCustom->insert_write(this);
  pollCustom->insert_error(this);

  priority_queue_insert(&taskScheduler, &m_taskTimeout, (cachedTime + rak::timer::from_seconds(60)).round_seconds());
}

void
Handshake::initialize_incoming(const rak::socket_address& sa) {
  m_peerInfo.set_incoming(true);
  m_peerInfo.set_socket_address(&sa);

  m_state = READ_INFO;

  pollCustom->open(this);
  pollCustom->insert_read(this);
  pollCustom->insert_error(this);

  // Use lower timeout here.
  priority_queue_insert(&taskScheduler, &m_taskTimeout, (cachedTime + rak::timer::from_seconds(60)).round_seconds());
}

void
Handshake::clear() {
  m_state = INACTIVE;

  priority_queue_erase(&taskScheduler, &m_taskTimeout);

  pollCustom->remove_read(this);
  pollCustom->remove_write(this);
  pollCustom->remove_error(this);
  pollCustom->close(this);
}

void
Handshake::event_read() {
  try {

    switch (m_state) {
    case READ_INFO:
      m_readBuffer.move_end(read_stream_throws(m_readBuffer.end(), handshake_size - m_readBuffer.size_end()));

      // Check the first byte as early as possible so we can
      // disconnect non-BT connections if they send less than 20 bytes.
      if (m_readBuffer.size_end() >= 1 && m_readBuffer.peek_8() != 19)
	throw close_connection();

      if (m_readBuffer.size_end() < part1_size)
	return;

      if (std::memcmp(m_readBuffer.position() + 1, m_protocol, 19) != 0)
	throw close_connection();

      m_readBuffer.move_position(20);

      // Should do some option field stuff here, for now just copy.
      m_readBuffer.read_range(m_peerInfo.get_options(), m_peerInfo.get_options() + 8);

      // Check the info hash.
      if (m_peerInfo.is_incoming()) {
	m_downloadInfo = m_manager->download_info(std::string(m_readBuffer.position(), m_readBuffer.position() + 20));

	if (m_downloadInfo == NULL)
	  throw close_connection();

	m_state = WRITE_FILL;
	m_readBuffer.move_position(20);
	
	pollCustom->remove_read(this);
	pollCustom->insert_write(this);

	return;

      } else {
	if (std::memcmp(m_downloadInfo->hash().c_str(), m_readBuffer.position(), 20) != 0)
	  throw close_connection();

	m_state = READ_PEER;
	m_readBuffer.move_position(20);
      }

    case READ_PEER:
      if (m_readBuffer.size_end() < handshake_size)
	m_readBuffer.move_end(read_stream_throws(m_readBuffer.end(), handshake_size - m_readBuffer.size_end()));

      if (m_readBuffer.size_end() < handshake_size)
	return;

      m_peerInfo.set_id(std::string(m_readBuffer.position(), m_readBuffer.position() + 20));
      m_readBuffer.move_position(20);

      m_manager->receive_succeeded(this);

      return;

    default:
      throw internal_error("Handshake::event_write() called in invalid state.");
    }

  } catch (network_error& e) {
    m_manager->receive_failed(this);
  }
}

void
Handshake::event_write() {
  try {

    switch (m_state) {
    case CONNECTING:
      if (get_fd().get_error())
	throw close_connection();
 
    case WRITE_FILL:
      m_writeBuffer.write_8(19);
      m_writeBuffer.write_range(m_protocol, m_protocol + 19);
      m_writeBuffer.write_range(m_peerInfo.get_options(), m_peerInfo.get_options() + 8);
      m_writeBuffer.write_range(m_downloadInfo->hash().c_str(), m_downloadInfo->hash().c_str() + 20);
      m_writeBuffer.write_range(m_downloadInfo->local_id().c_str(), m_downloadInfo->local_id().c_str() + 20);

      m_writeBuffer.prepare_end();

      if (m_writeBuffer.size_end() != handshake_size)
	throw internal_error("Handshake::event_write() wrong fill size.");

      m_state = WRITE_SEND;

    case WRITE_SEND:
      m_writeBuffer.move_position(write_stream_throws(m_writeBuffer.position(), m_writeBuffer.remaining()));

      if (m_writeBuffer.remaining())
	return;

      if (m_peerInfo.is_incoming())
	m_state = READ_PEER;
      else
	m_state = READ_INFO;

      pollCustom->remove_write(this);
      pollCustom->insert_read(this);

      return;

    default:
      throw internal_error("Handshake::event_read() called in invalid state.");
    }

  } catch (network_error& e) {
    m_manager->receive_failed(this);
  }
}

void
Handshake::event_error() {
  m_manager->receive_failed(this);
}

}
