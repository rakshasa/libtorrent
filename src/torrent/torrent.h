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
void      initialize();

// Clean up and close stuff. Stopping all torrents and waiting for
// them to finish is not required, but recommended.
void      cleanup();

bool      listen_open(uint16_t begin, uint16_t end);
void      listen_close();  

// Set the file descriptors we want to pool for R/W/E events. All
// fd_set's must be valid pointers.
void      mark(fd_set* readSet, fd_set* writeSet, fd_set* exceptSet, int* maxFd);

// Do work on the polled file descriptors.
void      work(fd_set* readSet, fd_set* writeSet, fd_set* exceptSet, int maxFd);

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

  TIME_CURRENT,            // Unix time. (usec)
  TIME_SELECT,             // Timeout for the next select call. (usec)

  RATE_WINDOW,             // Window size used to measure rate. (usec)
  RATE_START,              // Window size to use on new bursts of data. (usec)

  THROTTLE_ROOT_CONST_RATE  // Bytes per second, 0 for unlimited.
                            // (the code supports 0 for none, but not in this API)
} GValue;

typedef enum {
  LIBRARY_NAME
} GString;

int64_t     get(GValue t);
std::string get(GString t);

void        set(GValue t, int64_t v);
void        set(GString t, const std::string& s);


}

#endif
