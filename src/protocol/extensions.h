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

#ifndef LIBTORRENT_PROTOCOL_EXTENSIONS_H
#define LIBTORRENT_PROTOCOL_EXTENSIONS_H

#include <string>
#include <vector>

#include <torrent/exceptions.h>
#include <torrent/object.h>

#include "download/download_info.h"

namespace torrent {

class ProtocolExtension {
public:
  typedef enum {
    HANDSHAKE = 0,
    UT_PEX,

    FIRST_INVALID,    // first invalid message ID

    SKIP_EXTENSION,
  } MessageType;

  typedef std::vector<SocketAddressCompact> PEXList;

  // Buffer that holds extension messages.
  // Recipient must call clear() when done with the buffer.
  struct Buffer {
    Buffer()                        : m_data(NULL), m_end(NULL), m_copied(false) {}
    Buffer(char* data, char* end)   : m_data(data), m_end(end),  m_copied(false) {}

    char*      data() const         { return m_data; }
    char*      end() const          { return m_end; }

    bool       copied() const       { return m_copied; }
    bool       empty() const        { return m_data == NULL; }
    size_t     length() const       { return m_end - m_data; }

    void       clear();
    void       set(char* data, char* end, bool copied);

  private:
    char*   m_data;
    char*   m_end;
    bool    m_copied;  // Used to indicate if buffer held by PCB is copied and needs to be deleted after transmission.
  };

  static const int    flag_default      = 0x1;
  static const int    flag_initial_pex  = 0x2;
  static const int    flag_received_pex = 0x4;

  static const char*  message_keys[FIRST_INVALID];

  // Number of extensions we support, not counting handshake.
  static const int    extension_count = FIRST_INVALID - HANDSHAKE - 1;

  ProtocolExtension() : m_flags(0), m_readType(FIRST_INVALID), m_read(NULL) { reset(); }
  ~ProtocolExtension() { if (m_read != NULL) delete[] m_read; }

  // Create default extension object, with all extensions disabled.
  // Useful for eliminating checks whether peer supports extensions at all.
  static ProtocolExtension make_default() { ProtocolExtension extension; extension.m_flags |= flag_default; return extension; }

  static Buffer       generate_handshake_message(bool pexEnable);
  static Buffer       generate_toggle_message(MessageType t, bool on);
  static Buffer       generate_ut_pex_message(const PEXList& added, const PEXList& removed);

  // Return peer's extension ID for the given extension type, or 0 if
  // disabled by peer.
  uint8_t             id(int t) const;

  // General information about peer from extension handshake.
  uint32_t            max_queue_length() const      { return m_maxQueueLength; }

  // Handle reading extension data from peer.
  void                read_start(int type, uint32_t length, bool skip);
  uint32_t            read(const uint8_t* buffer, uint32_t length, PeerInfo* peerInfo);
  uint32_t            read_need() const             { return m_readLeft; }

  bool                is_complete() const           { return m_readLeft == 0; }
  bool                is_invalid() const            { return m_readType == FIRST_INVALID; }

  bool                is_default() const            { return m_flags & flag_default; }

  // Initial PEX message after peer enables PEX needs to send full list
  // of peers instead of the delta list, so keep track of that.
  bool                is_initial_pex() const        { return m_flags & flag_initial_pex; }
  bool                is_received_pex() const       { return m_flags & flag_received_pex; }

  void                clear_initial_pex()           { m_flags &= ~flag_initial_pex; }
  void                set_received_pex()            { m_flags |= flag_received_pex; }

  void                reset();

private:
  void                parse_handshake(const Object& message, PeerInfo* peerInfo);
  void                parse_ut_pex(const Object& message, PeerInfo* peerInfo);

  void                peer_toggle_extension(int type, bool active);

  // Map of IDs peer uses for each extension message type, excluding HANDSHAKE.
  uint8_t             m_idMap[extension_count];

  uint32_t            m_maxQueueLength;

  int                 m_flags;

  uint8_t             m_readType;
  uint32_t            m_readLeft;
  char*               m_read;
  char*               m_readEnd;
};

inline void
ProtocolExtension::Buffer::clear() {
  if (!empty())
    delete[] m_data;

  m_data = m_end = NULL;
  m_copied = false;
}

inline void
ProtocolExtension::Buffer::set(char* data, char* end, bool copied) {
  m_data = data;
  m_end = end;
  m_copied = copied;
}

inline uint8_t
ProtocolExtension::id(int t) const {
  if (t == HANDSHAKE)
    return 0;

  if (t - 1 >= extension_count)
    throw internal_error("ProtocolExtension::id message type out of range.");

  return m_idMap[t - 1];
}

}

#endif
