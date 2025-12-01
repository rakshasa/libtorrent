#include "config.h"

#include "log.h"

#include "torrent/exceptions.h"
#include "torrent/hash_string.h"

#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <zlib.h>

#include <algorithm>
#include <fstream>
#include <functional>
#include <memory>
#include <mutex>

namespace torrent {

struct log_cache_entry {
  using outputs_type = log_group::outputs_type;

  bool equal_outputs(const outputs_type& out) const { return out == outputs; }

  void allocate(unsigned int count) { cache_first = new log_slot[count]; cache_last = cache_first + count; }
  void clear()                      { delete [] cache_first; cache_first = NULL; cache_last = NULL; }

  outputs_type outputs;
  log_slot*    cache_first;
  log_slot*    cache_last;
};

struct log_gz_output {
  log_gz_output(const char* filename, bool append) :
      gz_file(gzopen(filename, append ? "a" : "w"), gzclose) {}

  bool is_valid() const { return gz_file != Z_NULL; }

  // bool set_buffer(unsigned size) { return gzbuffer(gz_file.get(), size) == 0; }

  std::unique_ptr<gzFile_s, decltype(&gzclose)> gz_file;
};

using log_cache_list  = std::vector<log_cache_entry>;
using log_child_list  = std::vector<std::pair<int, int>>;

log_group_list  log_groups;
log_output_list log_outputs;

static log_cache_list  log_cache;
static log_child_list  log_children;
static std::mutex      log_mutex;

static const char log_level_char[] = { 'C', 'E', 'W', 'N', 'I', 'D' };

// Removing logs always triggers a check if we got any un-used
// log_output objects.

static void
log_update_child_cache(int index) {
  auto first =
    std::find_if(log_children.begin(), log_children.end(), [index](const auto& pair) { return pair >= std::make_pair(index, 0); });

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

static void
log_rebuild_cache() {
  std::for_each(log_groups.begin(), log_groups.end(), std::mem_fn(&log_group::clear_cached_outputs));

  for (int i = 0; i < LOG_GROUP_MAX_SIZE; i++)
    log_update_child_cache(i);

  // Clear the cache...
  std::for_each(log_cache.begin(), log_cache.end(), std::mem_fn(&log_cache_entry::clear));
  log_cache.clear();

  for (auto& idx : log_groups) {
    const log_group::outputs_type& use_outputs = idx.cached_outputs();

    if (use_outputs == 0) {
      idx.set_cached(NULL, NULL);
      continue;
    }

    auto cache_itr = std::find_if(log_cache.begin(), log_cache.end(), [&use_outputs](const auto& entry) {
      return entry.equal_outputs(use_outputs);
    });
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

    idx.set_cached(cache_itr->cache_first, cache_itr->cache_last);
  }
}

void
log_group::internal_print(const HashString* hash, const char* subsystem, const void* dump_data, size_t dump_size, const char* fmt, ...) {
  va_list ap;
  const unsigned int buffer_size = 4096;
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

  auto lock = std::scoped_lock(log_mutex);

  std::for_each(m_first, m_last, [this, &buffer, first](const auto& elem) {
    return elem(buffer, std::distance(buffer, first), std::distance(log_groups.begin(), this));
  });
  if (dump_data != NULL) {
    std::for_each(m_first, m_last, [dump_data, dump_size](const auto& log) {
      return log(static_cast<const char*>(dump_data), dump_size, -1);
    });
  }
}

#define LOG_CASCADE(parent) LOG_CHILDREN_CASCADE(parent, parent)
#define LOG_LINK(parent, child) log_children.emplace_back(parent, child)

#define LOG_CHILDREN_CASCADE(parent, subgroup)                          \
  log_children.emplace_back(parent + LOG_ERROR,    subgroup + LOG_CRITICAL); \
  log_children.emplace_back(parent + LOG_WARN,     subgroup + LOG_ERROR); \
  log_children.emplace_back(parent + LOG_NOTICE,   subgroup + LOG_WARN); \
  log_children.emplace_back(parent + LOG_INFO,     subgroup + LOG_NOTICE); \
  log_children.emplace_back(parent + LOG_DEBUG,    subgroup + LOG_INFO);

#define LOG_CHILDREN_SUBGROUP(parent, subgroup)                         \
  log_children.emplace_back(parent + LOG_CRITICAL, subgroup + LOG_CRITICAL); \
  log_children.emplace_back(parent + LOG_ERROR,    subgroup + LOG_ERROR); \
  log_children.emplace_back(parent + LOG_WARN,     subgroup + LOG_WARN);  \
  log_children.emplace_back(parent + LOG_NOTICE,   subgroup + LOG_NOTICE); \
  log_children.emplace_back(parent + LOG_INFO,     subgroup + LOG_INFO);  \
  log_children.emplace_back(parent + LOG_DEBUG,    subgroup + LOG_DEBUG);

void
log_initialize() {
  auto lock = std::scoped_lock(log_mutex);

  LOG_CASCADE(LOG_CRITICAL);

  LOG_CASCADE(LOG_PEER_CRITICAL);
  LOG_CASCADE(LOG_SOCKET_CRITICAL);
  LOG_CASCADE(LOG_STORAGE_CRITICAL);
  LOG_CASCADE(LOG_THREAD_CRITICAL);
  LOG_CASCADE(LOG_TORRENT_CRITICAL);

  LOG_CHILDREN_CASCADE(LOG_CRITICAL, LOG_PEER_CRITICAL);
  LOG_CHILDREN_CASCADE(LOG_CRITICAL, LOG_SOCKET_CRITICAL);
  LOG_CHILDREN_CASCADE(LOG_CRITICAL, LOG_STORAGE_CRITICAL);
  LOG_CHILDREN_CASCADE(LOG_CRITICAL, LOG_THREAD_CRITICAL);
  LOG_CHILDREN_CASCADE(LOG_CRITICAL, LOG_TORRENT_CRITICAL);

  LOG_LINK(LOG_CONNECTION, LOG_CONNECTION_BIND);
  LOG_LINK(LOG_CONNECTION, LOG_CONNECTION_FD);
  LOG_LINK(LOG_CONNECTION, LOG_CONNECTION_FILTER);
  LOG_LINK(LOG_CONNECTION, LOG_CONNECTION_HANDSHAKE);
  LOG_LINK(LOG_CONNECTION, LOG_CONNECTION_LISTEN);

  LOG_LINK(LOG_DHT, LOG_DHT_ERROR);
  LOG_LINK(LOG_DHT, LOG_DHT_CONTROLLER);
  LOG_LINK(LOG_DHT_ALL, LOG_DHT_ERROR);
  LOG_LINK(LOG_DHT_ALL, LOG_DHT_CONTROLLER);
  LOG_LINK(LOG_DHT_ALL, LOG_DHT_NODE);
  LOG_LINK(LOG_DHT_ALL, LOG_DHT_ROUTER);
  LOG_LINK(LOG_DHT_ALL, LOG_DHT_SERVER);

  std::sort(log_children.begin(), log_children.end());

  log_rebuild_cache();
}

void
log_cleanup() {
  auto lock = std::scoped_lock(log_mutex);

  std::fill(log_groups.begin(), log_groups.end(), log_group());

  log_outputs.clear();
  log_children.clear();

  std::for_each(log_cache.begin(), log_cache.end(), std::mem_fn(&log_cache_entry::clear));
  log_cache.clear();
}

static log_output_list::iterator
log_find_output_name(const char* name) {
  auto itr = log_outputs.begin();
  auto last = log_outputs.end();

  while (itr != last && itr->first != name)
    itr++;

  return itr;
}

void
log_open_output(const char* name, const log_slot& slot) {
  auto lock = std::scoped_lock(log_mutex);

  if (log_outputs.size() >= log_group::max_size_outputs()) {
    throw input_error("Cannot open more than 64 log output handlers.");
  }

  auto itr = log_find_output_name(name);

  if (itr == log_outputs.end()) {
    log_outputs.emplace_back(name, slot);
  } else {
    // by replacing the "write" slot binding, the old file gets closed
    // (handles are shared pointers)
    itr->second = slot;
  }

  log_rebuild_cache();
}

void
log_close_output(const char* name) {
  auto lock = std::scoped_lock(log_mutex);

  auto itr = log_find_output_name(name);

  if (itr != log_outputs.end())
    log_outputs.erase(itr);
}

void
log_add_group_output(int group, const char* name) {
  auto lock = std::scoped_lock(log_mutex);

  auto itr = log_find_output_name(name);
  size_t index = std::distance(log_outputs.begin(), itr);

  if (itr == log_outputs.end())
    throw input_error("Log name not found: '" + std::string(name) + "'");

  if (index >= log_group::max_size_outputs())
    throw input_error("Cannot add more log group outputs.");

  log_groups[group].set_output_at(index, true);
  log_rebuild_cache();
}

void
log_remove_group_output([[maybe_unused]] int group, [[maybe_unused]] const char* name) {
}

// The log_children list is <child, group> since we build the output
// cache by crawling from child to parent.
void
log_add_child(int group, int child) {
  auto lock = std::scoped_lock(log_mutex);

  if (std::find(log_children.begin(), log_children.end(), std::make_pair(group, child)) != log_children.end())
    return;

  log_children.emplace_back(group, child);
  std::sort(log_children.begin(), log_children.end());

  log_rebuild_cache();
}

void
log_remove_child([[maybe_unused]] int group, [[maybe_unused]] int child) {
  // Remove from all groups, then modify all outputs.
}

// TODO: Add lock for file writes.

static void
log_file_write(const std::shared_ptr<std::ofstream>& outfile, const char* data, size_t length, int group) {
  // Add group name, data, etc as flags.

  // Normal groups are nul-terminated strings.
  if (group >= LOG_NON_CASCADING) {
    *outfile << this_thread::cached_seconds().count() << ' ' << data << '\n';
  } else if (group >= 0) {
    *outfile << this_thread::cached_seconds().count() << ' ' << log_level_char[group % 6] << ' ' << data << '\n';
  } else if (group == -1) {
    *outfile << "---DUMP---" << '\n';
    if (length != 0) {
      outfile->rdbuf()->sputn(data, length);
      *outfile << '\n';
    }
    *outfile << "---END---" << '\n';
  }
}

static void
log_gz_file_write(const std::shared_ptr<log_gz_output>& outfile, const char* data, size_t length, int group) {
  char buffer[64];

  // Normal groups are nul-terminated strings.
  if (group >= 0) {
    int buffer_length = snprintf(buffer, 64,
                                 (group >= LOG_NON_CASCADING) ? ("%" PRIi64 " ") : ("%" PRIi64 " %c "),
                                 static_cast<int64_t>(this_thread::cached_seconds().count()),
                                 log_level_char[group % 6]);

    if (buffer_length > 0)
      gzwrite(outfile->gz_file.get(), buffer, buffer_length);

    gzwrite(outfile->gz_file.get(), data, length);
    gzwrite(outfile->gz_file.get(), "\n", 1);

  } else if (group == -1) {
    gzwrite(outfile->gz_file.get(), "---DUMP---\n", sizeof("---DUMP---\n") - 1);

    if (length != 0)
      gzwrite(outfile->gz_file.get(), data, length);

    gzwrite(outfile->gz_file.get(), "---END---\n", sizeof("---END---\n") - 1);
  }
}

void
log_open_file_output(const char* name, const char* filename, bool append) {
  std::ios_base::openmode mode = std::ofstream::out;
  if (append)
    mode |= std::ofstream::app;
  auto outfile = std::make_shared<std::ofstream>(filename, mode);

  if (!outfile->good())
    throw input_error("Could not open log file '" + std::string(filename) + "'.");

  log_open_output(name, [outfile](auto d, auto l, auto g) { log_file_write(outfile, d, l, g); });
}

void
log_open_gz_file_output(const char* name, const char* filename, bool append) {
  auto outfile = std::make_shared<log_gz_output>(filename, append);

  if (!outfile->is_valid())
    throw input_error("Could not open log gzip file '" + std::string(filename) + "'.");

  // if (!outfile->set_buffer(1 << 14))
  //   throw input_error("Could not set gzip log file buffer size.");

  log_open_output(name, [outfile](auto d, auto l, auto g) { log_gz_file_write(outfile, d, l, g); });
}

} // namespace torrent
