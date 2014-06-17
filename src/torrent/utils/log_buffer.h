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

#ifndef LIBTORRENT_UTILS_LOG_BUFFER_H
#define LIBTORRENT_UTILS_LOG_BUFFER_H

#include <string>
#include <deque>
#include <pthread.h>
#include lt_tr1_functional
#include <torrent/common.h>

namespace torrent {

struct log_entry {
  log_entry(int32_t t, int32_t grp, const std::string& msg) : timestamp(t), group(grp), message(msg) {}

  bool        is_older_than(int32_t t) const { return timestamp < t; }
  bool        is_younger_than(int32_t t) const { return timestamp > t; }
  bool        is_younger_or_same(int32_t t) const { return timestamp >= t; }

  int32_t     timestamp;
  int32_t     group;
  std::string message;
};

class LIBTORRENT_EXPORT log_buffer : private std::deque<log_entry> {
public:
  typedef std::deque<log_entry>       base_type;
  typedef std::function<void ()> slot_void;

  using base_type::iterator;
  using base_type::const_iterator;
  using base_type::reverse_iterator;
  using base_type::const_reverse_iterator;

  using base_type::begin;
  using base_type::end;
  using base_type::rbegin;
  using base_type::rend;
  using base_type::front;
  using base_type::back;

  using base_type::empty;
  using base_type::size;

  log_buffer() : m_max_size(200) { pthread_mutex_init(&m_lock, NULL); }

  unsigned int        max_size() const { return m_max_size; }
  
  // Always lock before calling any function.
  void                lock()   { pthread_mutex_lock(&m_lock); }
  void                unlock() { pthread_mutex_unlock(&m_lock); }

  const_iterator      find_older(int32_t older_than);

  void                lock_and_set_update_slot(const slot_void& slot) { lock(); m_slot_update = slot; unlock(); }

  void                lock_and_push_log(const char* data, size_t length, int group);

private:
  pthread_mutex_t     m_lock;
  unsigned int        m_max_size;
  slot_void           m_slot_update;
};

}

#endif
