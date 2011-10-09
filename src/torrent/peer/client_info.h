// libTorrent - BitTorrent library
// Copyright (C) 2005-2011, Jari Sundell
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

#ifndef LIBTORRENT_PEER_CLIENT_INFO_H
#define LIBTORRENT_PEER_CLIENT_INFO_H

#include <torrent/common.h>

namespace torrent {

class LIBTORRENT_EXPORT ClientInfo {
public:
  friend class ClientList;

  typedef enum {
    TYPE_UNKNOWN,
    TYPE_AZUREUS,
    TYPE_COMPACT,
    TYPE_MAINLINE,
    TYPE_MAX_SIZE
  } id_type;

  struct info_type {
    const char*         m_shortDescription;
  };    

  static const uint32_t max_key_size = 2;
  static const uint32_t max_version_size = 4;

  id_type             type() const                            { return m_type; }

  const char*         key() const                             { return m_key; }
  const char*         version() const                         { return m_version; }
  const char*         upper_version() const                   { return m_upperVersion; }

  const char*         short_description() const               { return m_info->m_shortDescription; }
  void                set_short_description(const char* str)  { m_info->m_shortDescription = str; }

  static unsigned int key_size(id_type id);
  static unsigned int version_size(id_type id);

  // The intersect/disjoint postfix indicates what kind of equivalence
  // comparison we get when using !less && !greater.
  static bool         less_intersects(const ClientInfo& left, const ClientInfo& right);
  static bool         less_disjoint(const ClientInfo& left, const ClientInfo& right);

  static bool         greater_intersects(const ClientInfo& left, const ClientInfo& right);
  static bool         greater_disjoint(const ClientInfo& left, const ClientInfo& right);

  static bool         intersects(const ClientInfo& left, const ClientInfo& right);
  static bool         equal_to(const ClientInfo& left, const ClientInfo& right);

protected:
  void                set_type(id_type t)                     { m_type = t; }
  
  info_type*          info() const                            { return m_info; }
  void                set_info(info_type* ptr)                { m_info = ptr; }

  char*               mutable_key()                           { return m_key; }
  char*               mutable_version()                       { return m_version; }
  char*               mutable_upper_version()                 { return m_upperVersion; }

private:
  id_type             m_type;
  char                m_key[max_key_size];

  // The client version. The ClientInfo object in ClientList bounds
  // the versions that this object applies to. When the user searches
  // the ClientList for a client id, m_version will be set to the
  // actual client version.
  char                m_version[max_version_size];
  char                m_upperVersion[max_version_size];

  // We don't really care about cleaning up this as deleting an entry
  // form ClientList shouldn't happen.
  info_type*          m_info;
};

}

#endif
