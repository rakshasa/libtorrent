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

#define __STDC_FORMAT_MACROS

#include "log.h"
#include "log_buffer.h"

#include "globals.h"
#include "torrent/exceptions.h"
#include "torrent/hash_string.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <zlib.h>

#include <algorithm>
#include <fstream>
#include <functional>
#include <memory>
#include lt_tr1_functional
#include lt_tr1_memory

namespace torrent {

struct log_cache_entry {
  typedef log_group::outputs_type outputs_type;

  bool equal_outputs(const outputs_type& out) const { return out == outputs; }

  void allocate(unsigned int count) { cache_first = new log_slot[count]; cache_last = cache_first + count; }
  void clear()                      { delete [] cache_first; cache_first = NULL; cache_last = NULL; }

  outputs_type outputs;
  log_slot*    cache_first;
  log_slot*    cache_last;
};

struct log_gz_output {
  log_gz_output(const char* filename) { gz_file = gzopen(filename, "w"); }
  ~log_gz_output() { if (gz_file != NULL) gzclose(gz_file); }

  bool is_valid() { return gz_file != Z_NULL; }

  // bool set_buffer(unsigned size) { return gzbuffer(gz_file, size) == 0; }

  gzFile gz_file;
};

typedef std::vector<log_cache_entry>                   log_cache_list;
typedef std::vector<std::pair<int, int> >              log_child_list;
typedef std::vector<log_slot>                          log_slot_list;
typedef std::vector<std::pair<std::string, log_slot> > log_output_list;

log_output_list log_outputs;
log_child_list  log_children;
log_cache_list  log_cache;
log_group_list  log_groups;

pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

const char log_level_char[] = { 'C', 'E', 'W', 'N', 'I', 'D' };

// Removing logs always triggers a check if we got any un-used
// log_output objects.

void
log_update_child_cache(int index) {
  log_child_list::const_iterator first =
    std::find_if(log_children.begin(), log_children.end(),
                 std::bind2nd(std::greater_equal<std::pair<int, int> >(), std::make_pair(index, 0)));

  if (first == log_children.end())
    return;

  const log_group::outputs_type& outputs = log_groups[index].cached_outputs();

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

  for (int idx = 0, last = log_groups.size(); idx != last; idx++) {
    const log_group::outputs_type& use_outputs = log_groups[idx].cached_outputs();

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
      cache_itr->allocate(use_outputs.count());

      log_slot* dest_itr = cache_itr->cache_first;

      for (size_t index = 0; index < log_outputs.size(); index++) {
        if (use_outputs[index])
          *dest_itr++ = log_outputs[index].second;
      }
    }

    log_groups[idx].set_cached(cache_itr->cache_first, cache_itr->cache_last);
  }
}

void
log_group::internal_print(const HashString* hash, const char* subsystem, const void* dump_data, size_t dump_size, const char* fmt, ...) {
  va_list ap;
  unsigned int buffer_size = 4096;
  char buffer[buffer_size];
  char* first = buffer;

  if (subsystem != NULL) {
    if (hash != NULL) {
      first = hash_string_to_hex(*hash, first);
      first += snprintf(first, 4096 - (first - buffer), "->%s: ", subsystem);
    } else {
      first += snprintf(first, 4096 - (first - buffer), "%s: ", subsystem);
    }
  }

  va_start(ap, fmt);
  int count = vsnprintf(first, 4096 - (first - buffer), fmt, ap);
  first += std::min<unsigned int>(count, buffer_size - 1);
  va_end(ap);

  if (count <= 0)
    return;

  pthread_mutex_lock(&log_mutex);

  std::for_each(m_first, m_last, std::bind(&log_slot::operator(),
                                           std::placeholders::_1,
                                           (const char*)buffer,
                                           std::distance(buffer, first),
                                           std::distance(log_groups.begin(), this)));
  if (dump_data != NULL) {
    std::for_each(m_first, m_last, std::bind(&log_slot::operator(),
                                             std::placeholders::_1,
                                             (const char*)dump_data,
                                             dump_size,
                                             -1));
  }

  pthread_mutex_unlock(&log_mutex);
}

#define LOG_CASCADE(parent) LOG_CHILDREN_CASCADE(parent, parent)
#define LOG_LINK(parent, child) log_children.push_back(std::make_pair(parent, child))

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
  pthread_mutex_lock(&log_mutex);

  LOG_CASCADE(LOG_CRITICAL);

  LOG_CASCADE(LOG_CONNECTION_CRITICAL);
  LOG_CASCADE(LOG_PEER_CRITICAL);
  LOG_CASCADE(LOG_SOCKET_CRITICAL);
  LOG_CASCADE(LOG_STORAGE_CRITICAL);
  LOG_CASCADE(LOG_THREAD_CRITICAL);
  LOG_CASCADE(LOG_TRACKER_CRITICAL);
  LOG_CASCADE(LOG_TORRENT_CRITICAL);

  LOG_CHILDREN_CASCADE(LOG_CRITICAL, LOG_CONNECTION_CRITICAL);
  LOG_CHILDREN_CASCADE(LOG_CRITICAL, LOG_PEER_CRITICAL);
  LOG_CHILDREN_CASCADE(LOG_CRITICAL, LOG_SOCKET_CRITICAL);
  LOG_CHILDREN_CASCADE(LOG_CRITICAL, LOG_STORAGE_CRITICAL);
  LOG_CHILDREN_CASCADE(LOG_CRITICAL, LOG_THREAD_CRITICAL);
  LOG_CHILDREN_CASCADE(LOG_CRITICAL, LOG_TRACKER_CRITICAL);
  LOG_CHILDREN_CASCADE(LOG_CRITICAL, LOG_TORRENT_CRITICAL);

  LOG_LINK(LOG_DHT_ALL, LOG_DHT_MANAGER);
  LOG_LINK(LOG_DHT_ALL, LOG_DHT_NODE);
  LOG_LINK(LOG_DHT_ALL, LOG_DHT_ROUTER);
  LOG_LINK(LOG_DHT_ALL, LOG_DHT_SERVER);

  std::sort(log_children.begin(), log_children.end());

  log_rebuild_cache();
  pthread_mutex_unlock(&log_mutex);
}

void
log_cleanup() {
  pthread_mutex_lock(&log_mutex);

  std::fill(log_groups.begin(), log_groups.end(), log_group());

  log_outputs.clear();
  log_children.clear();

  std::for_each(log_cache.begin(), log_cache.end(), std::mem_fun_ref(&log_cache_entry::clear));
  log_cache.clear();

  pthread_mutex_unlock(&log_mutex);
}

log_output_list::iterator
log_find_output_name(const char* name) {
  log_output_list::iterator itr = log_outputs.begin();
  log_output_list::iterator last = log_outputs.end();

  while (itr != last && itr->first != name)
    itr++;

  return itr;
}

void
log_open_output(const char* name, log_slot slot) {
  pthread_mutex_lock(&log_mutex);

  if (log_outputs.size() >= log_group::max_size_outputs()) {
    pthread_mutex_unlock(&log_mutex);
    throw input_error("Cannot open more than 64 log output handlers.");
  }
  
  if (log_find_output_name(name) != log_outputs.end()) {
    pthread_mutex_unlock(&log_mutex);
    throw input_error("Log name already used.");
  }

  log_outputs.push_back(std::make_pair(name, slot));
  log_rebuild_cache();

  pthread_mutex_unlock(&log_mutex);
}

void
log_close_output(const char* name) {
}

void
log_add_group_output(int group, const char* name) {
  pthread_mutex_lock(&log_mutex);

  log_output_list::iterator itr = log_find_output_name(name);
  size_t index = std::distance(log_outputs.begin(), itr);

  if (itr == log_outputs.end()) {
    pthread_mutex_unlock(&log_mutex);
    throw input_error("Log name not found.");
  }

  if (index >= log_group::max_size_outputs()) {
    pthread_mutex_unlock(&log_mutex);
    throw input_error("Cannot add more log group outputs.");
  }

  log_groups[group].set_output_at(index, true);
  log_rebuild_cache();

  pthread_mutex_unlock(&log_mutex);
}

void
log_remove_group_output(int group, const char* name) {
}

// The log_children list is <child, group> since we build the output
// cache by crawling from child to parent.
void
log_add_child(int group, int child) {
  pthread_mutex_lock(&log_mutex);
  if (std::find(log_children.begin(), log_children.end(), std::make_pair(group, child)) != log_children.end())
    return;

  log_children.push_back(std::make_pair(group, child));
  std::sort(log_children.begin(), log_children.end());

  log_rebuild_cache();
  pthread_mutex_unlock(&log_mutex);
}

void
log_remove_child(int group, int child) {
  // Remove from all groups, then modify all outputs.
}

void
log_file_write(std::shared_ptr<std::ofstream>& outfile, const char* data, size_t length, int group) {
  // Add group name, data, etc as flags.

  // Normal groups are nul-terminated strings.
  if (group >= LOG_NON_CASCADING) {
    *outfile << cachedTime.seconds() << ' ' << data << std::endl;
  } else if (group >= 0) {
    *outfile << cachedTime.seconds() << ' ' << log_level_char[group % 6] << ' ' << data << std::endl;
  } else if (group == -1) {
    *outfile << "---DUMP---" << std::endl;
    if (length != 0) {
      outfile->rdbuf()->sputn(data, length);
      *outfile << std::endl;
    }
    *outfile << "---END---" << std::endl;
  }
}

void
log_gz_file_write(std::shared_ptr<log_gz_output>& outfile, const char* data, size_t length, int group) {
  char buffer[64];

  // Normal groups are nul-terminated strings.
  if (group >= 0) {
    const char* fmt = (group >= LOG_NON_CASCADING) ? ("%" PRIi32 " ") : ("%" PRIi32 " %c ");

    int buffer_length = snprintf(buffer, 64, fmt,
                                 cachedTime.seconds(),
                                 log_level_char[group % 6]);
    
    if (buffer_length > 0)
      gzwrite(outfile->gz_file, buffer, buffer_length);

    gzwrite(outfile->gz_file, data, length);
    gzwrite(outfile->gz_file, "\n", 1);

  } else if (group == -1) {
    gzwrite(outfile->gz_file, "---DUMP---\n", sizeof("---DUMP---\n") - 1);
    
    if (length != 0)
      gzwrite(outfile->gz_file, data, length);

    gzwrite(outfile->gz_file, "---END---\n", sizeof("---END---\n") - 1);
  }
}

void
log_open_file_output(const char* name, const char* filename) {
  std::shared_ptr<std::ofstream> outfile(new std::ofstream(filename));

  if (!outfile->good())
    throw input_error("Could not open log file '" + std::string(filename) + "'.");

  log_open_output(name, std::bind(&log_file_write, outfile,
                                  std::placeholders::_1,
                                  std::placeholders::_2,
                                  std::placeholders::_3));
}

void
log_open_gz_file_output(const char* name, const char* filename) {
  std::shared_ptr<log_gz_output> outfile(new log_gz_output(filename));

  if (!outfile->is_valid())
    throw input_error("Could not open log gzip file '" + std::string(filename) + "'.");

  // if (!outfile->set_buffer(1 << 14))
  //   throw input_error("Could not set gzip log file buffer size.");

  log_open_output(name, std::bind(&log_gz_file_write, outfile,
                                  std::placeholders::_1,
                                  std::placeholders::_2,
                                  std::placeholders::_3));
}

log_buffer*
log_open_log_buffer(const char* name) {
  log_buffer* buffer = new log_buffer;

  try {
    log_open_output(name, std::bind(&log_buffer::lock_and_push_log, buffer,
                                    std::placeholders::_1,
                                    std::placeholders::_2,
                                    std::placeholders::_3));
    return buffer;

  } catch (torrent::input_error& e) {
    delete buffer;
    throw;
  }
}

}
