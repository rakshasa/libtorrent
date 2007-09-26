// libTorrent - BitTorrent library
// Copyright (C) 2005-2007, Jari Sundell
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

#include <limits>
#include <sstream>

#include "download/available_list.h"
#include "download/connection_list.h"
#include "download/download_main.h"
#include "protocol/peer_connection_base.h"
#include "torrent/connection_manager.h"
#include "torrent/object.h"
#include "torrent/object_stream.h"
#include "torrent/peer/peer_info.h"
#include "tracker/tracker_http.h"
#include "manager.h"

#include "extensions.h"

namespace torrent {

const char* ProtocolExtension::message_keys[] = {
  "HANDSHAKE",
  "ut_pex",
};

void
ProtocolExtension::cleanup() {
//   if (is_default())
//     return;

  for (int t = HANDSHAKE + 1; t < FIRST_INVALID; t++)
    if (is_local_enabled(t))
      unset_local_enabled(t);
}

void
ProtocolExtension::set_local_enabled(int t) {
  if (is_local_enabled(t))
    return;

  m_flags |= flag_local_enabled_base << t;

  switch (t) {
  case UT_PEX:
//     m_download->info()->set_size_pex(download->info()->size_pex() + 1);
    break;
  default:
    break;
  }
}

void
ProtocolExtension::unset_local_enabled(int t) {
  if (!is_local_enabled(t))
    return;

  m_flags &= ~(flag_local_enabled_base << t);

  switch (t) {
  case UT_PEX:
//     m_download->info()->set_size_pex(download->info()->size_pex() + 1);
    break;
  default:
    break;
  }
}

DataBuffer
ProtocolExtension::generate_handshake_message() {
  Object map(Object::TYPE_MAP);
  Object message(Object::TYPE_MAP);

  map.insert_key(message_keys[UT_PEX], is_local_enabled(UT_PEX) ? 1 : 0);

  // Add "e" key if encryption is enabled, set it to 1 if we require
  // encryption for incoming connections, or 0 otherwise.
  if (manager->connection_manager()->encryption_options() & ConnectionManager::encryption_allow_incoming != 0)
    message.insert_key("e", manager->connection_manager()->encryption_options() & ConnectionManager::encryption_require != 0);

  message.insert_key("m", map);
  message.insert_key("p", manager->connection_manager()->listen_port());
  message.insert_key("v", "libTorrent " VERSION);
  message.insert_key("reqq", 2048);  // maximum request queue size

  char buffer[1024];
  object_buffer_t result = object_write_bencode_c(object_write_to_buffer, NULL, std::make_pair(buffer, buffer + sizeof(buffer)), &message);

  int length = result.second - buffer;
  char* copy = new char[length];
  memcpy(copy, buffer, length);

  return DataBuffer(copy, copy + length);
}

DataBuffer
ProtocolExtension::generate_toggle_message(ProtocolExtension::MessageType t, bool on) {
  // TODO: Check if we're accepting this message type?

  // Manually create bencoded map { "m" => { message_keys[t] => on ? t : 0 } }
  char* b = new char[32];
  unsigned int length = snprintf(b, 32, "d1:md%zu:%si%deee", strlen(message_keys[t]), message_keys[t], on ? t : 0);

  if (length > 32)
    throw internal_error("ProtocolExtension::toggle_message wrote past buffer.");

  return DataBuffer(b, b + length);
}

DataBuffer
ProtocolExtension::generate_ut_pex_message(const PEXList& added, const PEXList& removed) {
  if (added.empty() && removed.empty())
    return DataBuffer();

  int added_len   = added.size() * 6;
  int removed_len = removed.size() * 6;

  // Manually create bencoded map { "added" => added, "dropped" => dropped }
  char* buffer = new char[32 + added_len + removed_len];
  char* end = buffer;

  end += sprintf(end, "d5:added%d:", added_len);
  memcpy(end, added.begin()->c_str(), added_len);
  end += added_len;

  end += sprintf(end, "7:dropped%d:", removed_len);
  memcpy(end, removed.begin()->c_str(), removed_len);
  end += removed_len;

  *end++ = 'e';
  if (end - buffer > 32 + added_len + removed_len)
    throw internal_error("ProtocolExtension::ut_pex_message wrote beyond buffer.");

  return DataBuffer(buffer, end);
}

void
ProtocolExtension::read_start(int type, uint32_t length, bool skip) {
  if (is_default() || (type >= FIRST_INVALID))
    throw communication_error("Received invalid extension message.");

  if (m_read != NULL)
    throw internal_error("ProtocolExtension::read_start called in inconsistent state.");

  if (skip || !is_local_enabled(type))
    m_readType = SKIP_EXTENSION;
  else
    m_readType = type;

  m_readLeft = length;

  if (!skip)
    m_readEnd = m_read = new char[length];
}

uint32_t
ProtocolExtension::read(const uint8_t* buffer, uint32_t length) {
  if (length > m_readLeft)
    throw internal_error("ProtocolExtension::read string too long.");

  m_readLeft -= length;

  if (m_readType == SKIP_EXTENSION) {
    if (m_readLeft == 0)
      m_readType = FIRST_INVALID;

    return length;
  }

  memcpy(m_readEnd, buffer, length);
  m_readEnd += length;

  if (m_readLeft > 0)
    return length;

  std::stringstream s(std::string(m_read, m_readEnd));
  s.imbue(std::locale::classic());

  delete[] m_read;
  m_read = NULL;

  Object message;
  s >> message;

  if (s.fail() || !message.is_map())
    throw communication_error("Invalid extension message.");

  switch(m_readType) {
  case HANDSHAKE:
    parse_handshake(message);
    break;

  case UT_PEX:
    parse_ut_pex(message);
    break;

  default:
    throw internal_error("ProtocolExtension::down_extension_finished called with invalid extension type.");
  }

  m_readType = FIRST_INVALID;
  m_flags |= flag_received_ext;

  return length;
}

// Called whenever peer enables or disables an extension.
void
ProtocolExtension::peer_toggle_remote(int type, bool active) {
  if (type == UT_PEX) {
    // When ut_pex is enabled, the first peer exchange afterwards needs
    // to be a full message, not delta.
    if (active)
      m_flags |= flag_initial_pex;
  }
}

void
ProtocolExtension::parse_handshake(const Object& message) {
  if (message.has_key_map("m")) {
    const Object& idMap = message.get_key("m");

    for (int t = HANDSHAKE + 1; t < FIRST_INVALID; t++) {
      if (!idMap.has_key_value(message_keys[t]))
        continue;

      uint8_t id = idMap.get_key_value(message_keys[t]);

      if (id != m_idMap[t - 1]) {
        set_remote_supported(t);
        peer_toggle_remote(t, id != 0);

        m_idMap[t - 1] = id;
      }

    }
  }

  // If this is the first handshake, then disable any local extensions
  // not supported by remote.
  if (is_initial_handshake()) {
    for (int t = HANDSHAKE + 1; t < FIRST_INVALID; t++)
      if (!is_remote_supported(t))
        unset_local_enabled(t);
  }

  if (message.has_key_value("p")) {
    uint16_t port = message.get_key_value("p");

    if (port > 0)
      m_peerInfo->set_listen_port(port);
  }

  if (message.has_key_value("reqq"))
    m_maxQueueLength = message.get_key_value("reqq");

  m_flags &= ~flag_initial_handshake;
}

void
ProtocolExtension::parse_ut_pex(const Object& message) {
  // Ignore message if we're still in the handshake (no connection
  // yet), or no peers are present.

  // TODO: Check if pex is enabled?
  if (!message.has_key_string("added"))
    return;

  const std::string& peers = message.get_key_string("added");
  if (peers.empty())
    return;

  AddressList l;
  l.parse_address_compact(peers);
  l.sort();
  l.erase(std::unique(l.begin(), l.end()), l.end());
 
  m_download->connection_list()->set_difference(&l);
  m_download->peer_list()->available_list()->insert(&l);
}

}
