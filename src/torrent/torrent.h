// libTorrent - BitTorrent library
// Copyright (C) 2005-2006, Jari Sundell
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
#include <inttypes.h>
#include <torrent/download.h>

#include <sys/types.h>
#include <sys/select.h>

struct sockaddr;

namespace torrent {

class Bencode;
class Poll;
class Rate;
class ConnectionManager;

// Make sure you seed srandom and srand48 if available.
void                initialize(Poll* poll);

// Clean up and close stuff. Stopping all torrents and waiting for
// them to finish is not required, but recommended.
void                cleanup();

bool                listen_open(uint16_t begin, uint16_t end);
void                listen_close();  
uint16_t            listen_port();

int64_t             next_timeout();

// Calls this function when get_next_timeout() is zero or at
// semi-regular intervals when socket events accure. It updates the
// cached time and performs scheduled tasks.
//
// Bad name, find something better.
void                perform();

bool                is_inactive();

ConnectionManager*  connection_manager();

uint32_t            total_handshakes();

// These should really be unsigned, but there was a bug in the
// client. ;( Fix this later.
//
// 0 == UNLIMITED.
int32_t             down_throttle();
void                set_down_throttle(int32_t bytes);

int32_t             up_throttle();
void                set_up_throttle(int32_t bytes);

uint32_t            currently_unchoked();
uint32_t            max_unchoked();
void                set_max_unchoked(uint32_t count);

const Rate*         down_rate();
const Rate*         up_rate();

const char*         version();

// Disk access tuning.
uint32_t            hash_read_ahead();
void                set_hash_read_ahead(uint32_t bytes);

uint32_t            hash_interval();
void                set_hash_interval(uint32_t usec);

uint32_t            hash_max_tries();
void                set_hash_max_tries(uint32_t tries);

uint32_t            open_files();
uint32_t            max_open_files();
void                set_max_open_files(uint32_t size);

uint32_t            open_sockets();
uint32_t            max_open_sockets();
void                set_max_open_sockets(uint32_t size);

typedef std::list<Download> DList;
typedef std::list<std::string> EncodingList;

EncodingList*       encoding_list();

// Will always return a valid Download. On errors it
// throws. 'encodingList' contains a list of prefered encodings to use
// for file names.
Download            download_add(std::istream* s);
void                download_remove(Download d);

// Add all downloads to dlist. The client is responsible for clearing
// it before the call.
void                download_list(DList& dlist);

// Make sure you check the returned Download's is_valid().
Download            download_find(const std::string& infohash);

uint32_t            download_priority(Download d);
void                download_set_priority(Download d, uint32_t pri);

}

#endif
