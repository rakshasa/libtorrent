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

#include "log.h"
#include "globals.h"
#include "rak/algorithm.h"
#include "rak/timer.h"
#include "torrent/exceptions.h"
#include "torrent/download_info.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include <algorithm>
#include <fstream>
#include <functional>
#include <memory>
#include <tr1/functional>
#include <tr1/memory>

namespace std { using namespace tr1; }

namespace torrent {

typedef std::vector<log_slot> log_slot_list;

struct log_cache_entry {
  bool equal_outputs(uint64_t out) const { return out == outputs; }

  void allocate(unsigned int count) { cache_first = new log_slot[count]; cache_last = cache_first + count; }
  void clear()                      { delete [] cache_first; cache_first = NULL; cache_last = NULL; }

  uint64_t      outputs;
  log_slot*     cache_first;
  log_slot*     cache_last;
};

typedef std::vector<log_cache_entry>      log_cache_list;
typedef std::vector<std::pair<int, int> > log_child_list;

log_output_list log_outputs;
log_child_list  log_children;
log_cache_list  log_cache;
log_group_list  log_groups;

// Removing logs always triggers a check if we got any un-used
// log_output objects.

void
log_update_child_cache(int index) {
  log_child_list::const_iterator first =
    std::find_if(log_children.begin(), log_children.end(),
                 std::bind2nd(std::greater_equal<std::pair<int, int> >(), std::make_pair(index, 0)));

  if (first == log_children.end())
    return;

  uint64_t outputs = log_groups[index].cached_outputs();

  while (first != log_children.end() && first->first == index) {
    if ((outputs & log_groups[first->second].cached_outputs()) != outputs) {
      log_groups[first->second].set_cached_outputs(outputs | log_groups[first->second].cached_outputs());
      log_update_child_cache(first->second);
    }

    first++;
  }

  // If we got any circular connections re-do the update to ensure all
  // children have our new outputs.
  if (outputs != log_groups[index].cached_outputs())
    log_update_child_cache(index);
}

void
log_rebuild_cache() {
  std::for_each(log_groups.begin(), log_groups.end(), std::mem_fun_ref(&log_group::clear_cached_outputs));

  for (int i = 0; i < LOG_GROUP_MAX_SIZE; i++)
    log_update_child_cache(i);

  // Clear the cache...
  std::for_each(log_cache.begin(), log_cache.end(), std::mem_fun_ref(&log_cache_entry::clear));
  log_cache.clear();

  //for (log_group_list::iterator itr = log_groups.begin(), last = log_groups.end(); itr != last; itr++) {

  for (int idx = 0, last = log_groups.size(); idx != last; idx++) {
    uint64_t use_outputs = log_groups[idx].cached_outputs();

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
      cache_itr->allocate(rak::popcount_wrapper(use_outputs));

      log_slot* dest_itr = cache_itr->cache_first;

      for (log_output_list::iterator itr = log_outputs.begin(), last = log_outputs.end(); itr != last; itr++, use_outputs >>= 1) {
        if (use_outputs & 0x1)
          *dest_itr++ = itr->second;
      }
    }

    log_groups[idx].set_cached(cache_itr->cache_first, cache_itr->cache_last);
  }
}

void
log_group::internal_print(const char* fmt, ...) {
  va_list ap;
  unsigned int buffer_size = 4096;
  char buffer[buffer_size];

  va_start(ap, fmt);
  int count = vsnprintf(buffer, 4096, fmt, ap);
  va_end(ap);

  if (count >= 0)
    std::for_each(m_first, m_last, std::bind(&log_slot::operator(),
                                             std::placeholders::_1, buffer,
                                             std::min<unsigned int>(count, buffer_size - 1),
                                             std::distance(log_groups.begin(), this)));
}

void
log_group::internal_print_info(const DownloadInfo* info, const char* fmt, ...) {
  va_list ap;
  unsigned int buffer_size = 4096;
  char buffer[buffer_size];
  char* first = hash_string_to_hex(info->hash(), buffer);

  va_start(ap, fmt);
  int count = vsnprintf(first, 4096 - (first - buffer) , fmt, ap);
  va_end(ap);

  if (count >= 0)
    std::for_each(m_first, m_last, std::bind(&log_slot::operator(),
                                             std::placeholders::_1, buffer,
                                             std::min<unsigned int>(count, buffer_size - 1),
                                             std::distance(log_groups.begin(), this)));
}

#define LOG_CASCADE(parent) LOG_CHILDREN_CASCADE(parent, parent)

#define LOG_CHILDREN_CASCADE(parent, subgroup)                          \
  log_children.push_back(std::make_pair(parent + LOG_ERROR,    subgroup + LOG_CRITICAL)); \
  log_children.push_back(std::make_pair(parent + LOG_WARN,     subgroup + LOG_ERROR)); \
  log_children.push_back(std::make_pair(parent + LOG_NOTICE,   subgroup + LOG_WARN)); \
  log_children.push_back(std::make_pair(parent + LOG_INFO,     subgroup + LOG_NOTICE)); \
  log_children.push_back(std::make_pair(parent + LOG_DEBUG,    subgroup + LOG_INFO));

#define LOG_CHILDREN_SUBGROUP(parent, subgroup)                         \
  log_children.push_back(std::make_pair(parent + LOG_CRITICAL, subgroup + LOG_CRITICAL)); \
  log_children.push_back(std::make_pair(parent + LOG_ERROR,    subgroup + LOG_ERROR)); \
  log_children.push_back(std::make_pair(parent + LOG_WARN,     subgroup + LOG_WARN));  \
  log_children.push_back(std::make_pair(parent + LOG_NOTICE,   subgroup + LOG_NOTICE)); \
  log_children.push_back(std::make_pair(parent + LOG_INFO,     subgroup + LOG_INFO));  \
  log_children.push_back(std::make_pair(parent + LOG_DEBUG,    subgroup + LOG_DEBUG));

void
log_initialize() {
  LOG_CASCADE(LOG_CRITICAL);
  LOG_CASCADE(LOG_CONNECTION_CRITICAL);
  LOG_CASCADE(LOG_DHT_CRITICAL);
  LOG_CASCADE(LOG_PEER_CRITICAL);
  LOG_CASCADE(LOG_RPC_CRITICAL);
  LOG_CASCADE(LOG_STORAGE_CRITICAL);
  LOG_CASCADE(LOG_THREAD_CRITICAL);
  LOG_CASCADE(LOG_TRACKER_CRITICAL);
  LOG_CASCADE(LOG_TORRENT_CRITICAL);

  LOG_CHILDREN_SUBGROUP(LOG_CRITICAL, LOG_CONNECTION_CRITICAL);
  LOG_CHILDREN_SUBGROUP(LOG_CRITICAL, LOG_DHT_CRITICAL);
  LOG_CHILDREN_SUBGROUP(LOG_CRITICAL, LOG_RPC_CRITICAL);
  LOG_CHILDREN_SUBGROUP(LOG_CRITICAL, LOG_STORAGE_CRITICAL);
  LOG_CHILDREN_SUBGROUP(LOG_CRITICAL, LOG_THREAD_CRITICAL);
  LOG_CHILDREN_SUBGROUP(LOG_CRITICAL, LOG_TORRENT_CRITICAL);

  // Some groups are too verbose, and as such the parent is matched
  // against the log output level one above.
  LOG_CHILDREN_CASCADE(LOG_CRITICAL, LOG_PEER_CRITICAL);
  LOG_CHILDREN_CASCADE(LOG_CRITICAL, LOG_TRACKER_CRITICAL);

  std::sort(log_children.begin(), log_children.end());
  log_rebuild_cache();
}

void
log_cleanup() {
  log_groups.assign(log_group());
  log_outputs.clear();
  log_children.clear();

  std::for_each(log_cache.begin(), log_cache.end(), std::mem_fun_ref(&log_cache_entry::clear));
  log_cache.clear();
}

log_output_list::iterator
log_find_output_name(const char* name) {
  log_output_list::iterator itr = log_outputs.begin();
  log_output_list::iterator last = log_outputs.end();

  while (itr != last && itr->first != name)
    itr++;

  return itr;
}

// Add limit of 64 log entities...
void
log_open_output(const char* name, log_slot slot) {
  if (log_outputs.size() >= (size_t)std::numeric_limits<uint64_t>::digits)
    throw input_error("Cannot open more than 64 log output handlers.");

  if (log_find_output_name(name) != log_outputs.end())
    throw input_error("Log name already used.");

  log_outputs.push_back(std::make_pair(name, slot));
  log_rebuild_cache();
}

char log_level_char[] = { 'C', 'E', 'W', 'N', 'I', 'D' };

void
log_file_write(std::shared_ptr<std::ofstream>& outfile, const char* data, size_t length, int group) {
  // Add group name, data, etc as flags.
  *outfile << cachedTime.seconds() << ' ' << log_level_char[group % 6] << ' ' << data << std::endl;
}

// TODO: Allow for different write functions that prepend timestamps,
// etc.
void
log_open_file_output(const char* name, const char* filename) {
  std::shared_ptr<std::ofstream> outfile(new std::ofstream(filename));

  if (!outfile->good())
    throw input_error("Could not open log file '" + std::string(filename) + "'.");

  //  log_open_output(name, std::bind(&std::ofstream::write, outfile, std::placeholders::_1, std::placeholders::_2));
  log_open_output(name, std::bind(&log_file_write, outfile, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

void
log_close_output(const char* name) {
}

void
log_add_group_output(int group, const char* name) {
  log_output_list::iterator itr = log_find_output_name(name);

  if (itr == log_outputs.end())
    throw input_error("Log name not found.");
  
  log_groups[group].set_outputs(log_groups[group].outputs() | (0x1 << std::distance(log_outputs.begin(), itr)));
  log_rebuild_cache();
}

void
log_remove_group_output(int group, const char* name) {
}

// The log_children list is <child, group> since we build the output
// cache by crawling from child to parent.
void
log_add_child(int group, int child) {
  if (std::find(log_children.begin(), log_children.end(), std::make_pair(group, child)) != log_children.end())
    return;

  log_children.push_back(std::make_pair(group, child));
  std::sort(log_children.begin(), log_children.end());
  log_rebuild_cache();
}

void
log_remove_child(int group, int child) {
  // Remove from all groups, then modify all outputs.
}

}
