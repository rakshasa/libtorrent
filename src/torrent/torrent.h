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

typedef std::list<Download> DList;

// Make sure you seed srandom and srand48 if available.
void                initialize();

// Clean up and close stuff. Stopping all torrents and waiting for
// them to finish is not required, but recommended.
void                cleanup();

bool                listen_open(uint16_t begin, uint16_t end);
void                listen_close();  

// Set the file descriptors we want to pool for R/W/E events. All
// fd_set's must be valid pointers.
void                mark(fd_set* readSet, fd_set* writeSet, fd_set* exceptSet, int* maxFd);

// Do work on the polled file descriptors. Make sure this is called
// every time TIME_SELECT timeout has passed.
void                work(fd_set* readSet, fd_set* writeSet, fd_set* exceptSet, int maxFd);

bool                is_inactive();

const std::string&  get_ip();
void                set_ip(const std::string& addr);

// Bind the sockets to a specific network device.
const std::string&  get_bind();
void                set_bind(const std::string& addr);

uint16_t            get_listen_port();

unsigned int        get_total_handshakes();

int64_t             get_current_time();
int64_t             get_next_timeout();

// 0 == UNLIMITED.
unsigned int        get_read_throttle();
void                set_read_throttle(unsigned int bytes);

unsigned int        get_write_throttle();
void                set_write_throttle(unsigned int bytes);

std::string         get_version();

// Disk access tuning.
unsigned int        get_hash_read_ahead();
void                set_hash_read_ahead(unsigned int bytes);

unsigned int        get_max_open_files();
void                set_max_open_files(unsigned int size);

// The below API might/might not be cleaned up.

// Will always return a valid Download. On errors it throws.
Download  download_create(std::istream* s);

// Add all downloads to dlist. Make sure it's cleared.
void      download_list(DList& dlist);

// Make sure you check that the returned Download is_valid().
Download  download_find(const std::string& id);

void      download_remove(const std::string& id);

// Returns the bencode object, make sure you don't modify stuff you shouldn't
// touch. Make sure you don't copy the object, since it is very expensive.
Bencode&  download_bencode(const std::string& id);

}

#endif
