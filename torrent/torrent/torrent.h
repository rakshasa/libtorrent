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

// Will always return a valid Download. On errors it throws.
Download download_create(std::istream& s);

// Add all downloads to dlist. Make sure it's cleared.
void     download_list(DList& dlist);

// Make sure you check that it's valid.
Download download_find(const std::string& id);

// Variables, do stuff.
typedef enum {
  LISTEN_PORT,             // Returns 0 if it isn't listening on any port.
  HANDSHAKES_TOTAL,        // Number of handshakes currently being performed.
  SHUTDOWN_DONE,           // Returns 1 if all downloads have stopped.

  FILES_CHECK_WAIT,        // Wait between checks during torrent init. (usec)

  DEFAULT_PEERS_MIN,
  DEFAULT_PEERS_MAX,

  DEFAULT_UPLOADS_MAX,

  DEFAULT_CHOKE_CYCLE,

  HAS_EXCEPTION,

  TIME_CURRENT,            // Unix time. (usec)
  TIME_SELECT,             // Timeout for the next select call. (usec)

  RATE_WINDOW,             // Window size used to measure rate. (usec)
  RATE_START,              // Window size to use on new bursts of data. (usec)

  THROTTLE_ROOT_CONST_RATE, // Bytes per second, 0 for unlimited.
                            // (the code supports 0 for none, but not in this API)
  
  HTTP_GETS

} GValue;

typedef enum {
  LIBRARY_NAME,

  POP_EXCEPTION
} GString;

int64_t     get(GValue t);
std::string get(GString t);

}

#endif
