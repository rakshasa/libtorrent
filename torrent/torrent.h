#ifndef LIBTORRENT_TORRENT_H
#define LIBTORRENT_TORRENT_H

#include <list>
#include <string>
#include <inttypes.h>
#include <sys/types.h>
#include <iosfwd>

namespace torrent {

class Download;
class PeerConnection;

typedef std::list<Download*> DList;
typedef std::list<PeerConnection*> PList;

typedef DList::const_iterator DItr; 
typedef PList::const_iterator PItr; 

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

  THROTTLE_ROOT_CONST_RATE // Bytes per second, 0 for unlimited.
                           // (the code supports 0 for none, but not in this API)
} GValue;

typedef enum {
  LIBRARY_NAME,

  POP_EXCEPTION
} GString;

typedef enum {
  BYTES_DOWNLOADED,
  BYTES_UPLOADED,
  BYTES_TOTAL,
  BYTES_DONE,

  CHUNKS_DONE,
  CHUNKS_SIZE,
  CHUNKS_TOTAL,

  CHOKE_CYCLE,            // Interval between optimistic unchoking.

  RATE_UP,
  RATE_DOWN,

  PEERS_MIN,              // Stop initiating new connections when reached.
  PEERS_MAX,              // Maximum connections to accept.
  PEERS_CONNECTED,        // Connected peers.
  PEERS_NOT_CONNECTED,    // Known but not connected

  TRACKER_CONNECTING,
  TRACKER_TIMEOUT,        // Set this to 0 if you want to get stuff from the tracker.

  UPLOADS_MAX
} DValue;

typedef enum {
  BITFIELD_LOCAL,
  BITFIELD_SEEN,          // Char per chunk, max 255.

  INFO_NAME,

  TRACKER_MSG
} DString;

typedef enum {
  PEER_LOCAL_CHOKED,
  PEER_LOCAL_INTERESTED,

  PEER_REMOTE_CHOKED,
  PEER_REMOTE_INTERESTED,

  PEER_CHOKE_DELAYED,       // Sending of choke delayed.

  PEER_RATE_DOWN,         // Bytes per second.
  PEER_RATE_UP,

  PEER_PORT
} PValue;

typedef enum {
  PEER_ID,
  PEER_DNS,

  PEER_BITFIELD,

  PEER_INCOMING,       // 4 bytes in hardware byte order.
  PEER_OUTGOING
} PString;

// Call this once per session.
void initialize(int beginPort = 6881, int endPort = 6999);

// Stop all downloads, tell the tracker and flush all incoming
// and outgoing buffers. Do work until GVALUE_SHUTDOWN_DONE is
// true.
void shutdown();

// Clean up, close stuff. Calling and waiting for shutdown is
// not required, but highly recommended.
void cleanup();

// Set the file descriptors we want to pool for R/W/E events. All
// fd_set's must be valid pointers. Returns the highest fd.
int mark(fd_set* readSet, fd_set* writeSet, fd_set* exceptSet);

// Do work on the file descriptors.
void work(fd_set* readSet, fd_set* writeSet, fd_set* exceptSet);

// It will be parsed through a stream anyway, so no need to supply
// a function that handles char arrays. Just use std::stringstream.
DList::const_iterator create(std::istream& s);

// List container with all the current downloads.
const DList& downloads();

// List of all connected peers in 'd'. This list will change it's
// order frequently since sorting is used for many operations. It is
// highly recommended that whenever you grab the list you copy it and
// use the sort() function to get whatever order suits you.
const PList& peers(DList::const_iterator d);

// Call this once you've (preferably though not required) stopped the
// download. Do not use the iterator after calling this function,
// pre-increment it or something.
void remove(DList::const_iterator d);

bool start(DList::const_iterator d);
bool stop(DList::const_iterator d);

int64_t     get(GValue t);
int64_t     get(DList::const_iterator d, DValue t);
int64_t     get(PList::const_iterator p, PValue t);

std::string get(GString t);
std::string get(DList::const_iterator d, DString t);
std::string get(PList::const_iterator p, PString t);

void set(GValue t, int64_t v);
void set(DList::const_iterator d, DValue t, int64_t v);

void set(GString t, const std::string& s);
void set(DList::const_iterator d, DString t, const std::string& s);

}

#endif // LIBTORRENT_TORRENT_H
