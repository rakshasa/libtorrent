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

#ifndef LIBTORRENT_UTILS_LOG_FILES_H
#define LIBTORRENT_UTILS_LOG_FILES_H

#include <utility>
#include <torrent/common.h>

namespace torrent {

class LIBTORRENT_EXPORT log_file {
public:
  log_file(const char* str) : m_fd(-1), m_last_update(0), m_name(str) {}

  bool                is_open()         const { return m_fd != -1; }

  int                 file_descriptor() const { return m_fd; }
  int32_t             last_update()     const { return m_last_update; }
  const char*         name()            const { return m_name; }

  void                set_last_update(int32_t seconds) { m_last_update = seconds; }

  bool                open_file(const char* filename);
  void                close();

private:
  int                 m_fd;
  int32_t             m_last_update;

  const char*         m_name;
};

enum {
  LOG_MINCORE_STATS,
  LOG_CHOKE_CHANGES,
  LOG_MAX_SIZE
};

extern log_file log_files[LOG_MAX_SIZE] LIBTORRENT_EXPORT;

log_file* find_log_file(const char* name) LIBTORRENT_EXPORT;

//
// Internal to libtorrent:
//

// Update log files:
void log_mincore_stats_func(bool is_incore, bool new_index, bool& continous);
void log_mincore_stats_func_sync_success(int count);
void log_mincore_stats_func_sync_failed(int count);
void log_mincore_stats_func_sync_not_synced(int count);
void log_mincore_stats_func_sync_not_deallocated(int count);
void log_mincore_stats_func_alloc_failed(int count);
void log_mincore_stats_func_alloc(int velocity);
void log_mincore_stats_func_dealloc(int velocity);

struct weighted_connection;

void log_choke_changes_func_new(void* address, const char* title, int quota, int adjust);
void log_choke_changes_func_peer(void* address, const char* title, weighted_connection* data);
void log_choke_changes_func_allocate(void* address, const char* title, unsigned int index, uint32_t count, int dist);

}

#endif
