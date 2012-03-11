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

#include "config.h"

#include "log_buffer.h"

#include <functional>
#include <tr1/functional>

#include "globals.h"

namespace tr1 { using namespace std::tr1; }

namespace torrent {

// Rename function/args?
log_buffer::const_iterator
log_buffer::find_older(int32_t older_than) {
  if (empty() || !back().is_younger_than(older_than))
    return end();

  return std::find_if(begin(), end(), tr1::bind(&log_entry::is_younger_or_same, tr1::placeholders::_1, older_than));
}

void
log_buffer::lock_and_push_log(const char* data, size_t length, int group) {
  if (group < 0)
    return;

  lock();

  if (size() >= max_size())
    base_type::pop_front();

  base_type::push_back(log_entry(cachedTime.seconds(), group % 6, std::string(data, length)));

  if (m_slot_update)
    m_slot_update();

  unlock();
}

}
