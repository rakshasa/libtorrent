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

struct option_pair {
  const char* name;
  int         value;
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

option_pair option_list_log_group[] = {
  { "critical",            LOG_CRITICAL },
  { "error",               LOG_ERROR },
  { "warn",                LOG_WARN },
  { "notice",              LOG_NOTICE },
  { "info",                LOG_INFO },
  { "debug",               LOG_DEBUG },
  
  { "connection_critical", LOG_CONNECTION_CRITICAL },
  { "connection_error",    LOG_CONNECTION_ERROR },
  { "connection_warn",     LOG_CONNECTION_WARN },
  { "connection_notice",   LOG_CONNECTION_NOTICE },
  { "connection_info",     LOG_CONNECTION_INFO },
  { "connection_debug",    LOG_CONNECTION_DEBUG },
  
  { "dht_critical",        LOG_DHT_CRITICAL },
  { "dht_error",           LOG_DHT_ERROR },
  { "dht_warn",            LOG_DHT_WARN },
  { "dht_notice",          LOG_DHT_NOTICE },
  { "dht_info",            LOG_DHT_INFO },
  { "dht_debug",           LOG_DHT_DEBUG },
  
  { "rpc_critical",        LOG_RPC_CRITICAL },
  { "rpc_error",           LOG_RPC_ERROR },
  { "rpc_warn",            LOG_RPC_WARN },
  { "rpc_notice",          LOG_RPC_NOTICE },
  { "rpc_info",            LOG_RPC_INFO },
  { "rpc_debug",           LOG_RPC_DEBUG },
  
  { "storage_critical",    LOG_STORAGE_CRITICAL },
  { "storage_error",       LOG_STORAGE_ERROR },
  { "storage_warn",        LOG_STORAGE_WARN },
  { "storage_notice",      LOG_STORAGE_NOTICE },
  { "storage_info",        LOG_STORAGE_INFO },
  { "storage_debug",       LOG_STORAGE_DEBUG },

  { "thread_critical",     LOG_THREAD_CRITICAL },
  { "thread_error",        LOG_THREAD_ERROR },
  { "thread_warn",         LOG_THREAD_WARN },
  { "thread_notice",       LOG_THREAD_NOTICE },
  { "thread_info",         LOG_THREAD_INFO },
  { "thread_debug",        LOG_THREAD_DEBUG },
  
  { "tracker_critical",    LOG_TRACKER_CRITICAL },
  { "tracker_error",       LOG_TRACKER_ERROR },
  { "tracker_warn",        LOG_TRACKER_WARN },
  { "tracker_notice",      LOG_TRACKER_NOTICE },
  { "tracker_info",        LOG_TRACKER_INFO },
  { "tracker_debug",       LOG_TRACKER_DEBUG },
  
  { "torrent_critical",    LOG_TORRENT_CRITICAL },
  { "torrent_error",       LOG_TORRENT_ERROR },
  { "torrent_warn",        LOG_TORRENT_WARN },
  { "torrent_notice",      LOG_TORRENT_NOTICE },
  { "torrent_info",        LOG_TORRENT_INFO },
  { "torrent_debug",       LOG_TORRENT_DEBUG },
  
  { NULL, 0 }
};

option_pair* option_lists[OPTION_MAX_SIZE] = {
  option_list_connection,
  option_list_heuristics,
  option_list_heuristics_download,
  option_list_heuristics_upload,
  option_list_encryption,
  option_list_ip_filter,
  option_list_ip_tos,
  option_list_log_group,
  option_list_tracker_mode,
};

int
option_find_string(option_enum opt_enum, const char* name) {
  option_pair* itr = option_lists[opt_enum];
  
  do {
    if (std::strcmp(itr->name, name) == 0)
      return itr->value;
  } while ((++itr)->name != NULL);

  throw input_error("Invalid option name.");  
}

const char*
option_as_string(option_enum opt_enum, int value) {
  option_pair* itr = option_lists[opt_enum];
  
  do {
    if (itr->value == value)
      return itr->name;
  } while ((++itr)->name != NULL);

  throw input_error("Invalid option value.");  
}

torrent::Object
option_list_strings(option_enum opt_enum) {
  Object::list_type result;

  option_pair* itr = option_lists[opt_enum];

  while (itr->name != NULL)
    result.push_back(std::string(itr++->name));

  return Object::from_list(result);
}

}
