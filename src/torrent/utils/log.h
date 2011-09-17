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

#ifndef LIBTORRENT_UTILS_LOG_H
#define LIBTORRENT_UTILS_LOG_H

//#include <utility>
#include <tr1/array>
#include <tr1/functional>
#include <torrent/common.h>

namespace torrent {

// TODO: Add option_strings support.
enum {
  LOG_CRITICAL,
  LOG_ERROR,
  LOG_WARN,
  LOG_NOTICE,
  LOG_INFO,
  LOG_DEBUG,

  LOG_CONNECTION_CRITICAL,
  LOG_CONNECTION_ERROR,
  LOG_CONNECTION_WARN,
  LOG_CONNECTION_NOTICE,
  LOG_CONNECTION_INFO,
  LOG_CONNECTION_DEBUG,

  LOG_STORAGE_CRITICAL,
  LOG_STORAGE_ERROR,
  LOG_STORAGE_WARN,
  LOG_STORAGE_NOTICE,
  LOG_STORAGE_INFO,
  LOG_STORAGE_DEBUG,

  LOG_MAX_SIZE
};

struct log_cached_outputs;
typedef std::tr1::function<void (const char*)>   log_slot;

class LIBTORRENT_EXPORT log_group {
public:
  log_group() : m_outputs(0), m_first(NULL), m_last(NULL) {}

  bool valid() const { return m_first != NULL; }
  bool empty() const { return m_first == NULL; }

  void print(const char* fmt, ...);

  // Internal:
  uint64_t            outputs() const                    { return m_outputs; }
  void                set_outputs(uint64_t val)          { m_outputs = val; }

  void                set_cached(log_slot* f, log_slot* l) { m_first = f; m_last = l; }

private:
  uint64_t            m_outputs;

  log_slot*           m_first;
  log_slot*           m_last;
};

typedef std::tr1::array<log_group, LOG_MAX_SIZE> log_group_list;

extern log_group_list log_groups LIBTORRENT_EXPORT;

#define lt_log_print(group, fmt, ...) if (!log_groups[group].empty()) log_groups[group].print(fmt, __VA_ARGS__);

// Called by torrent::initialize().
void log_initialize() LIBTORRENT_EXPORT;
void log_cleanup() LIBTORRENT_EXPORT;

void log_open_output(const char* name, log_slot slot) LIBTORRENT_EXPORT;
void log_close_output(const char* name) LIBTORRENT_EXPORT;

void log_add_group_output(int group, const char* name) LIBTORRENT_EXPORT;
void log_remove_group_output(int group, const char* name) LIBTORRENT_EXPORT;

void log_add_child(int group, int child) LIBTORRENT_EXPORT;
void log_remove_child(int group, int child) LIBTORRENT_EXPORT;

}

#endif
