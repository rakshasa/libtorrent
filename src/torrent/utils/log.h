#ifndef LIBTORRENT_UTILS_LOG_H
#define LIBTORRENT_UTILS_LOG_H

#include <array>
#include <bitset>
#include <functional>
#include <string>
#include <vector>

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

  LOG_DHT_CRITICAL,
  LOG_DHT_ERROR,
  LOG_DHT_WARN,
  LOG_DHT_NOTICE,
  LOG_DHT_INFO,
  LOG_DHT_DEBUG,

  LOG_PEER_CRITICAL,
  LOG_PEER_ERROR,
  LOG_PEER_WARN,
  LOG_PEER_NOTICE,
  LOG_PEER_INFO,
  LOG_PEER_DEBUG,

  LOG_SOCKET_CRITICAL,
  LOG_SOCKET_ERROR,
  LOG_SOCKET_WARN,
  LOG_SOCKET_NOTICE,
  LOG_SOCKET_INFO,
  LOG_SOCKET_DEBUG,

  LOG_STORAGE_CRITICAL,
  LOG_STORAGE_ERROR,
  LOG_STORAGE_WARN,
  LOG_STORAGE_NOTICE,
  LOG_STORAGE_INFO,
  LOG_STORAGE_DEBUG,

  LOG_THREAD_CRITICAL,
  LOG_THREAD_ERROR,
  LOG_THREAD_WARN,
  LOG_THREAD_NOTICE,
  LOG_THREAD_INFO,
  LOG_THREAD_DEBUG,

  LOG_TRACKER_CRITICAL,
  LOG_TRACKER_ERROR,
  LOG_TRACKER_WARN,
  LOG_TRACKER_NOTICE,
  LOG_TRACKER_INFO,
  LOG_TRACKER_DEBUG,

  LOG_TORRENT_CRITICAL,
  LOG_TORRENT_ERROR,
  LOG_TORRENT_WARN,
  LOG_TORRENT_NOTICE,
  LOG_TORRENT_INFO,
  LOG_TORRENT_DEBUG,

  LOG_NON_CASCADING,

  LOG_CONNECTION,
  LOG_CONNECTION_BIND,
  LOG_CONNECTION_FD,
  LOG_CONNECTION_FILTER,
  LOG_CONNECTION_HANDSHAKE,
  LOG_CONNECTION_LISTEN,

  // TODO: Rename dht_all to just dht.
  LOG_DHT_ALL,
  LOG_DHT_MANAGER,
  LOG_DHT_NODE,
  LOG_DHT_ROUTER,
  LOG_DHT_SERVER,

  LOG_INSTRUMENTATION_MEMORY,
  LOG_INSTRUMENTATION_MINCORE,
  LOG_INSTRUMENTATION_CHOKE,
  LOG_INSTRUMENTATION_POLLING,
  LOG_INSTRUMENTATION_TRANSFERS,

  LOG_MOCK_CALLS,

  LOG_PEER_LIST_EVENTS,
  LOG_PEER_LIST_ADDRESS,

  LOG_PROTOCOL_PIECE_EVENTS,
  LOG_PROTOCOL_METADATA_EVENTS,
  LOG_PROTOCOL_NETWORK_ERRORS,
  LOG_PROTOCOL_STORAGE_ERRORS,

  LOG_RESUME_DATA,

  LOG_RPC_EVENTS,
  LOG_RPC_DUMP,

  LOG_SYSTEM,

  LOG_UI_EVENTS,

  LOG_GROUP_MAX_SIZE
};

#define lt_log_is_valid(log_group) (torrent::log_groups[log_group].valid())

#define lt_log_print(log_group, ...)                                    \
  { if (torrent::log_groups[log_group].valid())                         \
      torrent::log_groups[log_group].internal_print(NULL, NULL, NULL, 0, __VA_ARGS__); }

#define lt_log_print_info(log_group, log_info, log_subsystem, ...)      \
  { if (torrent::log_groups[log_group].valid())                         \
      torrent::log_groups[log_group].internal_print(&log_info->hash(), log_subsystem, NULL, 0, __VA_ARGS__); }

#define lt_log_print_data(log_group, log_data, log_subsystem, ...)      \
  { if (torrent::log_groups[log_group].valid())                         \
      torrent::log_groups[log_group].internal_print(&log_data->hash(), log_subsystem, NULL, 0, __VA_ARGS__); }

#define lt_log_print_dump(log_group, log_dump_data, log_dump_size, ...) \
  { if (torrent::log_groups[log_group].valid())                         \
      torrent::log_groups[log_group].internal_print(NULL, NULL, log_dump_data, log_dump_size, __VA_ARGS__); }

#define lt_log_print_hash(log_group, log_hash, log_subsystem, ...)      \
  { if (torrent::log_groups[log_group].valid())                         \
    torrent::log_groups[log_group].internal_print(&log_hash, log_subsystem, NULL, 0, __VA_ARGS__); }

#define lt_log_print_info_dump(log_group, log_dump_data, log_dump_size, log_info, log_subsystem, ...) \
  { if (torrent::log_groups[log_group].valid())                         \
      torrent::log_groups[log_group].internal_print(&log_info->hash(), log_subsystem, log_dump_data, log_dump_size, __VA_ARGS__); }

#define lt_log_print_subsystem(log_group, log_subsystem, ...)           \
  { if (torrent::log_groups[log_group].valid())                         \
      torrent::log_groups[log_group].internal_print(NULL, log_subsystem, NULL, 0, __VA_ARGS__); }

typedef std::function<void (const char*, unsigned int, int)> log_slot;

class LIBTORRENT_EXPORT log_group {
public:
  typedef std::bitset<64> outputs_type;

  log_group() : m_first(NULL), m_last(NULL) {
    m_outputs.reset();
    m_cached_outputs.reset();
  }

  bool                valid() const { return m_first != NULL; }
  bool                empty() const { return m_first == NULL; }

  size_t              size_outputs() const { return std::distance(m_first, m_last); }

  static size_t       max_size_outputs() { return 64; }

  //
  // Internal:
  //

  void                internal_print(const HashString* hash, const char* subsystem,
                                     const void* dump_data, size_t dump_size,
                                     const char* fmt, ...);

  const outputs_type& outputs() const                    { return m_outputs; }
  const outputs_type& cached_outputs() const             { return m_cached_outputs; }

  void                clear_cached_outputs()             { m_cached_outputs = m_outputs; }

  void                set_outputs(const outputs_type& val)        { m_outputs = val; }
  void                set_cached_outputs(const outputs_type& val) { m_cached_outputs = val; }

  void                set_output_at(size_t index, bool val)       { m_outputs[index] = val; }

  void                set_cached(log_slot* f, log_slot* l)        { m_first = f; m_last = l; }

private:
  outputs_type        m_outputs;
  outputs_type        m_cached_outputs;

  log_slot*           m_first;
  log_slot*           m_last;
};

typedef std::array<log_group, LOG_GROUP_MAX_SIZE> log_group_list;

extern log_group_list log_groups LIBTORRENT_EXPORT;

void log_initialize() LIBTORRENT_EXPORT;
void log_cleanup() LIBTORRENT_EXPORT;

void log_open_output(const char* name, log_slot slot) LIBTORRENT_EXPORT;
void log_close_output(const char* name) LIBTORRENT_EXPORT;
void log_close_output_str(const std::string name) LIBTORRENT_EXPORT;

void log_add_group_output(int group, const char* name) LIBTORRENT_EXPORT;
void log_remove_group_output(int group, const char* name) LIBTORRENT_EXPORT;

void log_add_child(int group, int child) LIBTORRENT_EXPORT;
void log_remove_child(int group, int child) LIBTORRENT_EXPORT;

void log_open_file_output(const char* name, const char* filename, bool append = false) LIBTORRENT_EXPORT;
void log_open_gz_file_output(const char* name, const char* filename, bool append = false) LIBTORRENT_EXPORT;

//
// Implementation:
//

inline void log_close_output_str(const std::string name) { log_close_output(name.c_str()); }

}

#endif
