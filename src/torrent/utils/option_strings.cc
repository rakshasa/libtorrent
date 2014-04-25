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

#include <algorithm>
#include <functional>
#include <cstring>

#include "torrent/connection_manager.h"
#include "torrent/object.h"
#include "torrent/download/choke_group.h"
#include "torrent/download/choke_queue.h"

#include "exceptions.h"
#include "download.h"
#include "log.h"
#include "option_strings.h"

namespace torrent {

struct option_single {
  unsigned int size;
  const char** name;
};

struct option_pair {
  const char*  name;
  unsigned int value;
};

option_pair option_list_connection[] = {
  { "leech",        Download::CONNECTION_LEECH },
  { "seed",         Download::CONNECTION_SEED },
  { "initial_seed", Download::CONNECTION_INITIAL_SEED },
  { "metadata",     Download::CONNECTION_METADATA },
  { NULL, 0 }
};

option_pair option_list_heuristics[] = {
  { "upload_leech",              choke_queue::HEURISTICS_UPLOAD_LEECH },
  { "upload_leech_dummy",        choke_queue::HEURISTICS_UPLOAD_LEECH_DUMMY },
  { "download_leech",            choke_queue::HEURISTICS_DOWNLOAD_LEECH },
  { "download_leech_dummy",      choke_queue::HEURISTICS_DOWNLOAD_LEECH_DUMMY },
  { "invalid",                   choke_queue::HEURISTICS_MAX_SIZE },
  { NULL, 0 }
};

option_pair option_list_heuristics_download[] = {
  { "download_leech",            choke_queue::HEURISTICS_DOWNLOAD_LEECH },
  { "download_leech_dummy",      choke_queue::HEURISTICS_DOWNLOAD_LEECH_DUMMY },
  { NULL, 0 }
};

option_pair option_list_heuristics_upload[] = {
  { "upload_leech",              choke_queue::HEURISTICS_UPLOAD_LEECH },
  { "upload_leech_dummy",        choke_queue::HEURISTICS_UPLOAD_LEECH_DUMMY },
  { NULL, 0 }
};

option_pair option_list_encryption[] = {
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

option_pair option_list_ip_filter[] = {
  { "unwanted",  PeerInfo::flag_unwanted },
  { "preferred", PeerInfo::flag_preferred },
  { NULL, 0 }
};

option_pair option_list_ip_tos[] = {
  { "default",     torrent::ConnectionManager::iptos_default },
  { "lowdelay",    torrent::ConnectionManager::iptos_lowdelay },
  { "throughput",  torrent::ConnectionManager::iptos_throughput },
  { "reliability", torrent::ConnectionManager::iptos_reliability },
  { "mincost",     torrent::ConnectionManager::iptos_mincost },
  { NULL, 0 }
};

option_pair option_list_tracker_mode[] = {
  { "normal",     choke_group::TRACKER_MODE_NORMAL },
  { "aggressive", choke_group::TRACKER_MODE_AGGRESSIVE },
  { NULL, 0 }
};

const char* option_list_log_group[] = {
  "critical",
  "error",
  "warn",
  "notice",
  "info",
  "debug",
  
  "connection_critical",
  "connection_error",
  "connection_warn",
  "connection_notice",
  "connection_info",
  "connection_debug",
  
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

  "instrumentation_memory",
  "instrumentation_mincore",
  "instrumentation_choke",
  "instrumentation_polling",
  "instrumentation_transfers",

  "peer_list_events",

  "protocol_piece_events",
  "protocol_metadata_events",
  "protocol_network_errors",
  "protocol_storage_errors",

  "rpc_events",
  "rpc_dump",

  "ui_events",

  NULL
};

const char* option_list_tracker_event[] = {
  "updated",
  "completed",
  "started",
  "stopped",
  "scrape",

  NULL
};

option_pair* option_pair_lists[OPTION_START_COMPACT] = {
  option_list_connection,
  option_list_heuristics,
  option_list_heuristics_download,
  option_list_heuristics_upload,
  option_list_encryption,
  option_list_ip_filter,
  option_list_ip_tos,
  option_list_tracker_mode,
};

#define OPTION_SINGLE_ENTRY(single_name) \
  { sizeof(single_name) / sizeof(const char*) - 1, single_name }

option_single option_single_lists[OPTION_SINGLE_SIZE] = {
  OPTION_SINGLE_ENTRY(option_list_log_group),
  OPTION_SINGLE_ENTRY(option_list_tracker_event),
};

int
option_find_string(option_enum opt_enum, const char* name) {
  if (opt_enum < OPTION_START_COMPACT) {
    option_pair* itr = option_pair_lists[opt_enum];
  
    do {
      if (std::strcmp(itr->name, name) == 0)
        return itr->value;
    } while ((++itr)->name != NULL);

  } else if (opt_enum < OPTION_MAX_SIZE) {
    const char** itr = option_single_lists[opt_enum - OPTION_START_COMPACT].name;
  
    do {
      if (std::strcmp(*itr, name) == 0)
        return std::distance(option_single_lists[opt_enum - OPTION_START_COMPACT].name, itr);
    } while (*++itr != NULL);
  }

  throw input_error("Invalid option name.");  
}

const char*
option_as_string(option_enum opt_enum, unsigned int value) {
  if (opt_enum < OPTION_START_COMPACT) {
    option_pair* itr = option_pair_lists[opt_enum];
  
    do {
      if (itr->value == value)
        return itr->name;
    } while ((++itr)->name != NULL);

  } else if (opt_enum < OPTION_MAX_SIZE) {
    if (value < option_single_lists[opt_enum - OPTION_START_COMPACT].size)
      return option_single_lists[opt_enum - OPTION_START_COMPACT].name[value];
  }

  throw input_error("Invalid option value.");  
}

torrent::Object
option_list_strings(option_enum opt_enum) {
  Object::list_type result;

  if (opt_enum < OPTION_START_COMPACT) {
    option_pair* itr = option_pair_lists[opt_enum];

    while (itr->name != NULL)
      result.push_back(std::string(itr++->name));

  } else if (opt_enum < OPTION_MAX_SIZE) {
    const char** itr = option_single_lists[opt_enum - OPTION_START_COMPACT].name;
  
    while (*itr != NULL) result.push_back(std::string(*itr++));
  }

  return Object::from_list(result);
}

}
