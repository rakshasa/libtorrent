#include "config.h"

#include <array>
#include <cstring>

#include "torrent/connection_manager.h"
#include "torrent/download.h"
#include "torrent/download/choke_group.h"
#include "torrent/download/choke_queue.h"
#include "torrent/exceptions.h"
#include "torrent/object.h"
#include "torrent/peer/peer_info.h"
#include "torrent/utils/option_strings.h"

namespace torrent {

struct option_single {
  unsigned int size;
  const char* const* name;
};

struct option_pair {
  const char*  name;
  unsigned int value;
};

constexpr option_pair option_list_connection_type[] = {
  { "leech",        Download::CONNECTION_LEECH },
  { "seed",         Download::CONNECTION_SEED },
  { "initial_seed", Download::CONNECTION_INITIAL_SEED },
  { "metadata",     Download::CONNECTION_METADATA },
  { NULL, 0 }
};

constexpr option_pair option_list_heuristics[] = {
  { "upload_leech",              choke_queue::HEURISTICS_UPLOAD_LEECH },
  { "upload_leech_experimental", choke_queue::HEURISTICS_UPLOAD_LEECH_EXPERIMENTAL },
  { "upload_seed",               choke_queue::HEURISTICS_UPLOAD_SEED },
  { "download_leech",            choke_queue::HEURISTICS_DOWNLOAD_LEECH },
  { "invalid",                   choke_queue::HEURISTICS_MAX_SIZE },
  { NULL, 0 }
};

constexpr option_pair option_list_heuristics_download[] = {
  { "download_leech",            choke_queue::HEURISTICS_DOWNLOAD_LEECH },
  { NULL, 0 }
};

constexpr option_pair option_list_heuristics_upload[] = {
  { "upload_leech",              choke_queue::HEURISTICS_UPLOAD_LEECH },
  { "upload_leech_experimental", choke_queue::HEURISTICS_UPLOAD_LEECH_EXPERIMENTAL },
  { "upload_seed",               choke_queue::HEURISTICS_UPLOAD_SEED },
  { NULL, 0 }
};

constexpr option_pair option_list_encryption[] = {
  { "none",             torrent::ConnectionManager::encryption_none },
  { "allow_incoming",   torrent::ConnectionManager::encryption_allow_incoming },
  { "try_outgoing",     torrent::ConnectionManager::encryption_try_outgoing },
  { "require",          torrent::ConnectionManager::encryption_require },
  { "require_RC4",      torrent::ConnectionManager::encryption_require_RC4 },
  { "require_rc4",      torrent::ConnectionManager::encryption_require_RC4 },
  { "enable_retry",     torrent::ConnectionManager::encryption_enable_retry },
  { "prefer_plaintext", torrent::ConnectionManager::encryption_prefer_plaintext },
  { NULL, 0 }
};

constexpr option_pair option_list_ip_filter[] = {
  { "unwanted",  PeerInfo::flag_unwanted },
  { "preferred", PeerInfo::flag_preferred },
  { NULL, 0 }
};

constexpr option_pair option_list_ip_tos[] = {
  { "default",     torrent::ConnectionManager::iptos_default },
  { "lowdelay",    torrent::ConnectionManager::iptos_lowdelay },
  { "throughput",  torrent::ConnectionManager::iptos_throughput },
  { "reliability", torrent::ConnectionManager::iptos_reliability },
  { "mincost",     torrent::ConnectionManager::iptos_mincost },
  { NULL, 0 }
};

constexpr option_pair option_list_tracker_mode[] = {
  { "normal",     choke_group::TRACKER_MODE_NORMAL },
  { "aggressive", choke_group::TRACKER_MODE_AGGRESSIVE },
  { NULL, 0 }
};

constexpr const char* option_list_handshake_connection[] = {
  "none",
  "incoming",
  "outgoing_normal",
  "outgoing_encrypted",
  "outgoing_proxy",
  "success",
  "dropped",
  "failed",
  "retry_plaintext",
  "retry_encrypted",

  nullptr
};

constexpr const char* option_list_log_group[] = {
  "critical",
  "error",
  "warn",
  "notice",
  "info",
  "debug",

  "dht_critical",
  "dht_error",
  "dht_warn",
  "dht_notice",
  "dht_info",
  "dht_debug",

  "peer_critical",
  "peer_error",
  "peer_warn",
  "peer_notice",
  "peer_info",
  "peer_debug",

  "socket_critical",
  "socket_error",
  "socket_warn",
  "socket_notice",
  "socket_info",
  "socket_debug",

  "storage_critical",
  "storage_error",
  "storage_warn",
  "storage_notice",
  "storage_info",
  "storage_debug",

  "thread_critical",
  "thread_error",
  "thread_warn",
  "thread_notice",
  "thread_info",
  "thread_debug",

  "tracker_critical",
  "tracker_error",
  "tracker_warn",
  "tracker_notice",
  "tracker_info",
  "tracker_debug",

  "torrent_critical",
  "torrent_error",
  "torrent_warn",
  "torrent_notice",
  "torrent_info",
  "torrent_debug",

  "__non_cascading__",

  "connection",
  "connection_bind",
  "connection_fd",
  "connection_filter",
  "connection_hanshake",
  "connection_listen",

  "dht_all",
  "dht_manager",
  "dht_node",
  "dht_router",
  "dht_server",

  "instrumentation_memory",
  "instrumentation_mincore",
  "instrumentation_choke",
  "instrumentation_polling",
  "instrumentation_transfers",

  "mock_calls",

  "net_dns",

  "peer_list_events",
  "peer_list_address",

  "protocol_piece_events",
  "protocol_metadata_events",
  "protocol_network_errors",
  "protocol_storage_errors",

  "resume_data",

  "rpc_events",
  "rpc_dump",

  "storage",
  "system",

  "tracker_dump",
  "tracker_events",
  "tracker_requests",

  "ui_events",

  NULL
};

constexpr const char* option_list_tracker_event[] = {
  "updated",
  "completed",
  "started",
  "stopped",
  "scrape",

  NULL
};

constexpr std::array option_pair_lists{
  option_list_connection_type,
  option_list_heuristics,
  option_list_heuristics_download,
  option_list_heuristics_upload,
  option_list_encryption,
  option_list_ip_filter,
  option_list_ip_tos,
  option_list_tracker_mode,
};
static_assert(option_pair_lists.size() == OPTION_START_COMPACT);

#define OPTION_SINGLE_ENTRY(single_name) \
  option_single{ sizeof(single_name) / sizeof(const char*) - 1, single_name }

constexpr std::array option_single_lists{
  OPTION_SINGLE_ENTRY(option_list_handshake_connection),
  OPTION_SINGLE_ENTRY(option_list_log_group),
  OPTION_SINGLE_ENTRY(option_list_tracker_event),
};
static_assert(option_single_lists.size() == OPTION_SINGLE_SIZE);

int
option_find_string(option_enum opt_enum, const char* name) {
  if (opt_enum < OPTION_START_COMPACT) {
    auto itr = option_pair_lists[opt_enum];

    do {
      if (std::strcmp(itr->name, name) == 0)
        return itr->value;
    } while ((++itr)->name != NULL);

  } else if (opt_enum < OPTION_MAX_SIZE) {
    auto itr = option_single_lists[opt_enum - OPTION_START_COMPACT].name;

    do {
      if (std::strcmp(*itr, name) == 0)
        return std::distance(option_single_lists[opt_enum - OPTION_START_COMPACT].name, itr);
    } while (*++itr != NULL);
  }

  throw input_error("Invalid option name.");
}

const char*
option_to_string(option_enum opt_enum, unsigned int value, const char* not_found) {
  if (opt_enum < OPTION_START_COMPACT) {
    auto itr = option_pair_lists[opt_enum];

    do {
      if (itr->value == value)
        return itr->name;
    } while ((++itr)->name != NULL);

  } else if (opt_enum < OPTION_MAX_SIZE) {
    if (value < option_single_lists[opt_enum - OPTION_START_COMPACT].size)
      return option_single_lists[opt_enum - OPTION_START_COMPACT].name[value];
  }

  return not_found;
}

const char*
option_to_string_or_throw(option_enum opt_enum, unsigned int value, const char* not_found) {
  const char* result = option_to_string(opt_enum, value, NULL);

  if (result == NULL)
    throw input_error(not_found);
  else
    return result;
}

const char*
option_as_string(option_enum opt_enum, unsigned int value) {
  const char* result = option_to_string(opt_enum, value, NULL);

  if (result == NULL)
    throw input_error("Invalid option value.");
  else
    return result;
}

torrent::Object
option_list_strings(option_enum opt_enum) {
  Object::list_type result;

  if (opt_enum < OPTION_START_COMPACT) {
    auto itr = option_pair_lists[opt_enum];

    while (itr->name != NULL)
      result.emplace_back(std::string(itr++->name));

  } else if (opt_enum < OPTION_MAX_SIZE) {
    auto itr = option_single_lists[opt_enum - OPTION_START_COMPACT].name;

    while (*itr != NULL)
      result.emplace_back(std::string(*itr++));
  }

  return Object::from_list(result);
}

} // namespace torrent
