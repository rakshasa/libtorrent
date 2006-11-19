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

#include <cstring>

#include "client_info.h"

namespace torrent {

unsigned int
ClientInfo::key_size(id_type id) {
  switch (id) {
  case TYPE_AZUREUS:
    return 2;
  case TYPE_COMPACT:
  case TYPE_MAINLINE:
    return 1;

  default:
    return 0;
  }
}

unsigned int
ClientInfo::version_size(id_type id) {
  switch (id) {
  case TYPE_AZUREUS:
    return 4;
  case TYPE_COMPACT:
  case TYPE_MAINLINE:
    return 3;

  default:
    return 0;
  }
}

bool
ClientInfo::less_intersects(const ClientInfo& left, const ClientInfo& right) {
  if (left.type() > right.type())
    return false;
  else if (left.type() < right.type())
    return true;

  int keyComp = std::memcmp(left.key(), right.key(), ClientInfo::max_key_size);

  return
    keyComp < 0 ||
    (keyComp == 0 && std::memcmp(left.upper_version(), right.version(), ClientInfo::max_version_size) < 0);
}

bool
ClientInfo::less_disjoint(const ClientInfo& left, const ClientInfo& right) {
  if (left.type() > right.type())
    return false;
  else if (left.type() < right.type())
    return true;

  int keyComp = std::memcmp(left.key(), right.key(), ClientInfo::max_key_size);

  return
    keyComp < 0 ||
    (keyComp == 0 && std::memcmp(left.version(), right.upper_version(), ClientInfo::max_version_size) < 0);
}

bool
ClientInfo::greater_intersects(const ClientInfo& left, const ClientInfo& right) {
  return less_intersects(right, left);
}

bool
ClientInfo::greater_disjoint(const ClientInfo& left, const ClientInfo& right) {
  return less_disjoint(right, left);
}

bool
ClientInfo::intersects(const ClientInfo& left, const ClientInfo& right) {
  return
    left.type() == right.type() &&
    std::memcmp(left.key(), right.key(), ClientInfo::max_key_size) == 0 &&

    std::memcmp(left.version(), right.upper_version(), ClientInfo::max_version_size) <= 0 &&
    std::memcmp(left.upper_version(), right.version(), ClientInfo::max_version_size) >= 0;
}

inline bool
ClientInfo::equal_to(const ClientInfo& left, const ClientInfo& right) {
  return
    left.type() == right.type() &&
    std::memcmp(left.key(), right.key(), ClientInfo::max_key_size) == 0 &&

    std::memcmp(left.version(), right.version(), ClientInfo::max_version_size) == 0 &&
    std::memcmp(left.upper_version(), right.upper_version(), ClientInfo::max_version_size) == 0;
}

}
