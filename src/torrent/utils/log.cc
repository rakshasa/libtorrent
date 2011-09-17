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

#include "log.h"

#include <algorithm>
#include <functional>
#include <stdlib.h>
#include <vector>
#include <tr1/functional>

namespace std { using namespace tr1; }

namespace torrent {

typedef std::vector<log_slot> log_slot_list;

struct log_cache_entry {
  bool equal_outputs(uint64_t out) const { return out == outputs; }

  uint64_t      outputs;
  log_slot_list cache;
};

typedef std::vector<log_cache_entry>                   log_cache_list;
typedef std::vector<std::pair<std::string, log_slot> > log_output_list;

log_output_list                   log_outputs;
std::vector<std::pair<int, int> > log_children;

log_cache_list log_cache;
log_group_list log_groups;

// Removing logs always triggers a check if we got any un-used
// log_output objects.

// TODO: Copy to bitfield...
template<typename T>
inline int popcount_wrapper(T t) {
#if 1  //def __builtin_popcount
  if (std::numeric_limits<T>::digits <= std::numeric_limits<unsigned int>::digits)
    return __builtin_popcount(t);
  else
    return __builtin_popcountll(t);
#else
#warning __builtin_popcount not found.
  unsigned int count = 0;
  
  while (t) {
    count += t & 0x1;
    t >> 1;
  }

  return count;
#endif
}

void
log_rebuild_cache() {
  std::array<uint64_t, LOG_MAX_SIZE> use_cache;
  use_cache.assign(uint64_t());

  std::transform(log_groups.begin(), log_groups.end(), use_cache.begin(), std::bind(&log_group::outputs, std::placeholders::_1));

  // Build use_cache from children graph...
  for (log_group_list::const_iterator itr = log_groups.begin(), last = log_groups.end(); itr != last; itr++) {
    // Go down each edge unless that edge already has all our outputs set.
  }

  // Clear the cache...
  //  std::for_each(log_cache.begin(), log_cache.end(), std::mem_fun(&log_cache_entry::clear));
  log_cache.clear();

  // Build cache.
  //  for (log_group_list::iterator itr = log_groups.begin(), last = log_groups.end(); itr != last; itr++) {

  for (int idx = 0, last = log_groups.size(); idx != last; idx++) {
    uint64_t use_outputs = use_cache[idx];

    if (use_outputs == 0) {
      log_groups[idx].set_cached(NULL, NULL);
      continue;
    }

    log_cache_list::iterator cache_itr = 
      std::find_if(log_cache.begin(), log_cache.end(),
                   std::bind(&log_cache_entry::equal_outputs, std::placeholders::_1, use_outputs));
    
    if (cache_itr == log_cache.end()) {
      cache_itr = log_cache.insert(log_cache.end(), log_cache_entry());
      cache_itr->outputs = use_outputs;

      for (log_output_list::iterator itr = log_outputs.begin(), last = log_outputs.end(); itr != last; itr++, use_outputs >>= 1) {
        if (use_outputs & 0x1)
          cache_itr->cache.push_back(itr->second);
      }
    }

    log_groups[idx].set_cached(&*cache_itr->cache.begin(), &*cache_itr->cache.end());
  }
}

void
log_group::print(const char* fmt, ...) {
}

void
log_initialize() {
}

void
log_cleanup() {
}

// Add limit of 64 log entities...
void
log_open_output(const char* name, log_slot slot) {
}

void
log_close_output(const char* name) {
}

void
log_add_child(int group, int child) {

}

void
log_remove_child(int group, int child) {

}

}
