// libTorrent - BitTorrent library
// Copyright (C) 2005, Jari Sundell
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
#include <torrent/common.h>
#include <torrent/download.h>

#include <sys/types.h>
#include <sys/select.h>

namespace torrent {

class Bencode;
class Rate;

// Make sure you seed srandom and srand48 if available.
void                initialize();

// Clean up and close stuff. Stopping all torrents and waiting for
// them to finish is not required, but recommended.
void                cleanup();

bool                listen_open(uint16_t begin, uint16_t end);
void                listen_close();  

// mark and work might change when the current polling method is
// changed into a more modular one that will support both select and
// epoll.

// Set the file descriptors we want to pool for R/W/E events. All
// fd_set's must be valid pointers.
void                mark(fd_set* readSet, fd_set* writeSet, fd_set* exceptSet, int* maxFd);

// Do work on the polled file descriptors. Make sure this is called
// every time TIME_SELECT timeout has passed.
void                work(fd_set* readSet, fd_set* writeSet, fd_set* exceptSet, int maxFd);

bool                is_inactive();

// Change the address reported to the tracker.
const std::string&  get_address();
void                set_address(const std::string& addr);

// Bind the sockets to a specific network device.
const std::string&  get_bind_address();
void                set_bind_address(const std::string& addr);

uint16_t            get_listen_port();

unsigned int        get_total_handshakes();

int64_t             get_current_time();
int64_t             get_next_timeout();

// 0 == UNLIMITED.
int32_t             get_read_throttle();
void                set_read_throttle(int32_t bytes);

int32_t             get_write_throttle();
void                set_write_throttle(int32_t bytes);

void                set_throttle_interval(uint32_t usec);

const Rate&         get_read_rate();
const Rate&         get_write_rate();

char*               get_version();

// Disk access tuning.
uint32_t            get_hash_read_ahead();
void                set_hash_read_ahead(uint32_t bytes);

uint32_t            get_hash_interval();
void                set_hash_interval(uint32_t usec);

uint32_t            get_hash_max_tries();
void                set_hash_max_tries(uint32_t tries);

uint32_t            get_max_open_files();
void                set_max_open_files(uint32_t size);

uint32_t            get_open_sockets();
uint32_t            get_max_open_sockets();
void                set_max_open_sockets(uint32_t size);

// Will always return a valid Download. On errors it throws.
Download            download_add(std::istream* s);
void                download_remove(const std::string& infohash);

typedef std::list<Download> DList;

// Add all downloads to dlist. The client is responsible for clearing
// it before the call.
void                download_list(DList& dlist);

// Make sure you check the returned Download's is_valid().
Download            download_find(const std::string& infohash);

}

#endif
