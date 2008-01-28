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

#ifndef LIBTORRENT_TORRENT_H
#define LIBTORRENT_TORRENT_H

#include <list>
#include <string>
#include <torrent/common.h>
#include <torrent/download.h>

namespace torrent {

// Make sure you seed srandom and srand48 if available.
void                initialize(Poll* poll) LIBTORRENT_EXPORT;

// Clean up and close stuff. Stopping all torrents and waiting for
// them to finish is not required, but recommended.
void                cleanup() LIBTORRENT_EXPORT;

int64_t             next_timeout() LIBTORRENT_EXPORT;

// Calls this function when get_next_timeout() is zero or at
// semi-regular intervals when socket events accure. It updates the
// cached time and performs scheduled tasks.
//
// Bad name, find something better.
void                perform() LIBTORRENT_EXPORT;

bool                is_inactive() LIBTORRENT_EXPORT;

ChunkManager*       chunk_manager() LIBTORRENT_EXPORT;
ClientList*         client_list() LIBTORRENT_EXPORT;
ConnectionManager*  connection_manager() LIBTORRENT_EXPORT;
DhtManager*         dht_manager() LIBTORRENT_EXPORT;

uint32_t            total_handshakes() LIBTORRENT_EXPORT;

Throttle*           down_throttle_global() LIBTORRENT_EXPORT;
Throttle*           up_throttle_global() LIBTORRENT_EXPORT;

uint32_t            currently_unchoked() LIBTORRENT_EXPORT;
uint32_t            max_unchoked() LIBTORRENT_EXPORT;
void                set_max_unchoked(uint32_t count) LIBTORRENT_EXPORT;

uint32_t            download_unchoked() LIBTORRENT_EXPORT;
uint32_t            max_download_unchoked() LIBTORRENT_EXPORT;
void                set_max_download_unchoked(uint32_t count) LIBTORRENT_EXPORT;

const Rate*         down_rate() LIBTORRENT_EXPORT;
const Rate*         up_rate() LIBTORRENT_EXPORT;

const char*         version() LIBTORRENT_EXPORT;

// Disk access tuning.
uint32_t            hash_read_ahead() LIBTORRENT_EXPORT;
void                set_hash_read_ahead(uint32_t bytes) LIBTORRENT_EXPORT;

uint32_t            hash_interval() LIBTORRENT_EXPORT;
void                set_hash_interval(uint32_t usec) LIBTORRENT_EXPORT;

uint32_t            hash_max_tries() LIBTORRENT_EXPORT;
void                set_hash_max_tries(uint32_t tries) LIBTORRENT_EXPORT;

uint32_t            open_files() LIBTORRENT_EXPORT;
uint32_t            max_open_files() LIBTORRENT_EXPORT;
void                set_max_open_files(uint32_t size) LIBTORRENT_EXPORT;

typedef std::list<Download> DList;
typedef std::list<std::string> EncodingList;

EncodingList*       encoding_list() LIBTORRENT_EXPORT;

// Will always return a valid Download. On errors it
// throws. 'encodingList' contains a list of prefered encodings to use
// for file names.
//
// The Object must be on the heap allocated with 'new'. If
// 'download_add' throws the client must handle the deletion, else it
// is done by 'download_remove'.
//
// Might consider redesigning that...
Download            download_add(Object* s) LIBTORRENT_EXPORT;
void                download_remove(Download d) LIBTORRENT_EXPORT;

// Add all downloads to dlist. The client is responsible for clearing
// it before the call.
void                download_list(DList& dlist) LIBTORRENT_EXPORT;

// Make sure you check the returned Download's is_valid().
Download            download_find(const std::string& infohash) LIBTORRENT_EXPORT;

uint32_t            download_priority(Download d) LIBTORRENT_EXPORT;
void                download_set_priority(Download d, uint32_t pri) LIBTORRENT_EXPORT;

}

#endif
