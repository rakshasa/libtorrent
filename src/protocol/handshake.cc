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

#include "data/content.h"
#include "download/download_main.h"
#include "torrent/exceptions.h"
#include "torrent/connection_manager.h"
#include "torrent/poll.h"

#include "globals.h"
#include "manager.h"

#include "handshake.h"
#include "handshake_manager.h"

namespace torrent {

const char* Handshake::m_protocol = "BitTorrent protocol";

Handshake::Handshake(SocketFd fd, HandshakeManager* m) :
  m_state(INACTIVE),

  m_manager(m),
  m_peerInfo(NULL),
  m_download(NULL),

  m_readDone(false),
  m_writeDone(false) {

  set_fd(fd);

  m_readBuffer.reset();
  m_writeBuffer.reset();      

  m_taskTimeout.set_slot(rak::bind_mem_fn(m, &HandshakeManager::receive_failed, this));
}

Handshake::~Handshake() {
  if (m_taskTimeout.is_queued())
    throw internal_error("Handshake m_taskTimeout bork bork bork.");

  if (get_fd().is_valid())
    throw internal_error("Handshake dtor called but m_fd is still open.");

  delete m_peerInfo;
}

void
Handshake::initialize_outgoing(const rak::socket_address& sa, DownloadMain* d) {
  m_download = d;

  m_peerInfo = new PeerInfo();
  m_peerInfo->set_incoming(false);
  m_peerInfo->set_socket_address(&sa);

  m_state = CONNECTING;

  manager->poll()->open(this);
  manager->poll()->insert_write(this);
  manager->poll()->insert_error(this);

  priority_queue_insert(&taskScheduler, &m_taskTimeout, (cachedTime + rak::timer::from_seconds(60)).round_seconds());
}

void
Handshake::initialize_incoming(const rak::socket_address& sa) {
  m_peerInfo = new PeerInfo();
  m_peerInfo->set_incoming(true);
  m_peerInfo->set_socket_address(&sa);

  m_state = READ_INFO;

  manager->poll()->open(this);
  manager->poll()->insert_read(this);
  manager->poll()->insert_error(this);

  // Use lower timeout here.
  priority_queue_insert(&taskScheduler, &m_taskTimeout, (cachedTime + rak::timer::from_seconds(60)).round_seconds());
}

void
Handshake::clear() {
  m_state = INACTIVE;

  priority_queue_erase(&taskScheduler, &m_taskTimeout);

  manager->poll()->remove_read(this);
  manager->poll()->remove_write(this);
  manager->poll()->remove_error(this);
  manager->poll()->close(this);
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
        return m_manager->receive_failed(this);
      
      if (m_readBuffer.size_end() < part1_size)
        return;

      if (std::memcmp(m_readBuffer.position() + 1, m_protocol, 19) != 0)
        return m_manager->receive_failed(this);

      m_readBuffer.move_position(20);

      // Should do some option field stuff here, for now just copy.
      m_readBuffer.read_range(m_peerInfo->get_options(), m_peerInfo->get_options() + 8);

      // Check the info hash.
      if (m_peerInfo->is_incoming()) {
        m_download = m_manager->download_info(std::string(m_readBuffer.position(), m_readBuffer.position() + 20));

        if (m_download == NULL || !m_download->info()->accepting_new_peers())
          return m_manager->receive_failed(this);

        m_state = WRITE_FILL;
        m_readBuffer.move_position(20);

        manager->poll()->remove_read(this);
        manager->poll()->insert_write(this);

        return;

      } else {
        if (std::memcmp(m_download->info()->hash().c_str(), m_readBuffer.position(), 20) != 0)
          return m_manager->receive_failed(this);

        m_state = READ_PEER;
        m_readBuffer.move_position(20);
      }

    case READ_PEER:
      if (m_readBuffer.size_end() < handshake_size)
        m_readBuffer.move_end(read_stream_throws(m_readBuffer.end(), handshake_size - m_readBuffer.size_end()));

      if (m_readBuffer.size_end() < handshake_size)
        return;

      m_peerInfo->set_id(std::string(m_readBuffer.position(), m_readBuffer.position() + 20));
      m_readBuffer.move_position(20);

      if (m_peerInfo->get_id() == m_download->info()->local_id())
        return m_manager->receive_failed(this);

      m_readBuffer.reset();
      m_writeBuffer.reset();

      // The download is just starting so we're not sending any
      // bitfield.
      if (m_download->content()->bitfield()->is_all_unset()) {
        // Write a keep-alive message.
        m_writeBuffer.write_32(0);
        m_writeBuffer.prepare_end();

        // Skip writting the bitfield.
        m_writePos = m_download->content()->bitfield()->size_bytes();

      } else {
        m_writeBuffer.write_32(m_download->content()->bitfield()->size_bytes() + 1);
        m_writeBuffer.write_8(protocol_bitfield);
        m_writeBuffer.prepare_end();

        m_writePos = 0;
      }


      m_state = BITFIELD;
      manager->poll()->insert_write(this);

      // Give some extra time for reading/writing the bitfield.
      priority_queue_erase(&taskScheduler, &m_taskTimeout);
      priority_queue_insert(&taskScheduler, &m_taskTimeout, (cachedTime + rak::timer::from_seconds(120)).round_seconds());

      // Trigger event_write() directly and then skip straight to read
      // BITFIELD. This avoids going through polling for the first
      // write.
      event_write();

    case BITFIELD:
      if (m_bitfield.empty()) {
        m_readBuffer.move_end(read_stream_throws(m_readBuffer.end(), 5 - m_readBuffer.size_end()));

        // Received a keep-alive message which means we won't be
        // getting any bitfield.
        if (m_readBuffer.size_end() >= 4 && m_readBuffer.peek_32() == 0) {
          m_readBuffer.read_32();
          return read_done();
        }

        if (m_readBuffer.size_end() < 5)
          return;

        // Received a non-bitfield command.
        if (m_readBuffer.peek_8_at(4) != protocol_bitfield)
          return read_done();

        if (m_readBuffer.read_32() != m_download->content()->bitfield()->size_bytes() + 1)
          return m_manager->receive_failed(this);

        m_readBuffer.read_8();
        m_readPos = 0;

        m_bitfield.set_size_bits(m_download->content()->bitfield()->size_bits());
        m_bitfield.allocate();

        if (m_readBuffer.remaining() != 0)
          throw internal_error("Handshake::event_read() m_readBuffer.remaining() != 0.");

        // Copy any unread data to bitfield here.
      }

      // We're guaranteed that we still got bytes remaining to be
      // read of the bitfield.
      m_readPos += read_stream_throws(m_bitfield.begin() + m_readPos, m_bitfield.size_bytes() - m_readPos);
      
      if (m_readPos == m_bitfield.size_bytes())
        read_done();

      return;

    default:
      throw internal_error("Handshake::event_read() called in invalid state.");
    }

  } catch (network_error& e) {
    m_manager->receive_failed(this);
  }
}

void
Handshake::read_done() {
  m_readDone = true;
  manager->poll()->remove_read(this);

  if (m_bitfield.empty()) {
    m_bitfield.set_size_bits(m_download->content()->bitfield()->size_bytes());
    m_bitfield.allocate();
    m_bitfield.unset_all();

  } else {
    m_bitfield.update();
  }

  // Disconnect if both are seeders.

  if (m_writeDone)
    m_manager->receive_succeeded(this);
}

void
Handshake::event_write() {
  try {

    switch (m_state) {
    case CONNECTING:
      if (get_fd().get_error())
        return m_manager->receive_failed(this);
 
    case WRITE_FILL:
      m_writeBuffer.write_8(19);
      m_writeBuffer.write_range(m_protocol, m_protocol + 19);

      //       m_writeBuffer.write_range(m_peerInfo->get_options(), m_peerInfo->get_options() + 8);
      std::memset(m_writeBuffer.position(), 0, 8);
      m_writeBuffer.move_position(8);

      m_writeBuffer.write_range(m_download->info()->hash().c_str(), m_download->info()->hash().c_str() + 20);
      m_writeBuffer.write_range(m_download->info()->local_id().c_str(), m_download->info()->local_id().c_str() + 20);

      m_writeBuffer.prepare_end();

      if (m_writeBuffer.size_end() != handshake_size)
        throw internal_error("Handshake::event_write() wrong fill size.");

      m_state = WRITE_SEND;

    case WRITE_SEND:
      m_writeBuffer.move_position(write_stream_throws(m_writeBuffer.position(), m_writeBuffer.remaining()));

      if (m_writeBuffer.remaining())
        return;

      if (m_peerInfo->is_incoming())
        m_state = READ_PEER;
      else
        m_state = READ_INFO;

      manager->poll()->remove_write(this);
      manager->poll()->insert_read(this);

      return;

    case BITFIELD:
      if (m_writeBuffer.remaining())
        m_writeBuffer.move_position(write_stream_throws(m_writeBuffer.position(), m_writeBuffer.remaining()));
      
      if (m_writeBuffer.remaining())
        return;

      if (m_writePos != m_download->content()->bitfield()->size_bytes())
        m_writePos += write_stream_throws(m_download->content()->bitfield()->begin() + m_writePos,
                                          m_download->content()->bitfield()->size_bytes() - m_writePos);

      if (m_writePos == m_download->content()->bitfield()->size_bytes()) {
        m_writeDone = true;
        manager->poll()->remove_write(this);

        if (m_readDone)
          m_manager->receive_succeeded(this);
      }

      return;

    default:
      throw internal_error("Handshake::event_write() called in invalid state.");
    }

  } catch (network_error& e) {
    m_manager->receive_failed(this);
  }
}

void
Handshake::event_error() {
  if (m_state == INACTIVE)
    throw internal_error("Handshake::event_error() called on an inactive handshake.");

  m_manager->receive_failed(this);
}

}
