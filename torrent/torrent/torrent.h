#ifndef LIBTORRENT_TORRENT_H
#define LIBTORRENT_TORRENT_H

#include <list>
#include <torrent/common.h>
#include <torrent/download.h>

namespace torrent {

typedef std::list<Download> DList;

void initialize();

// Clean up and close stuff. Stopping all torrents and waiting for
// them to finish is not required, but recommended.
void cleanup();

bool listen_open(uint16_t begin, uint16_t end);
void listen_close();  

// Set the file descriptors we want to pool for R/W/E events. All
// fd_set's must be valid pointers. Returns the highest fd.
void mark(fd_set* readSet, fd_set* writeSet, fd_set* exceptSet, int* maxFd);

// Do work on the polled file descriptors.
void work(fd_set* readSet, fd_set* writeSet, fd_set* exceptSet);

// Add all downloads to dlist. Make sure it's cleared.
void downloads_list(DList& dlist);

// Make sure you check that it's valid.
Download downloads_find(const std::string& id);

// Returns 0 if it's not open.
uint32_t get_listen_port();

uint32_t get_handshakes_size();

uint64_t get_select_timeout();
uint64_t get_current_time(); // remove

uint32_t get_http_gets();

uint32_t get_root_throttle();
void     set_root_throttle(uint32_t v);

std::string get_library_name();
std::string get_next_message();

}

#endif
