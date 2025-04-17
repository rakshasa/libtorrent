#ifndef LIBTORRENT_COMMON_H
#define LIBTORRENT_COMMON_H

#include <cinttypes>
#include <cstddef>

struct sockaddr;
struct sockaddr_in;
struct sockaddr_in6;
struct sockaddr_un;

namespace torrent {

enum priority_enum {
  PRIORITY_OFF = 0,
  PRIORITY_NORMAL,
  PRIORITY_HIGH
};

enum tracker_enum {
  TRACKER_NONE,
  TRACKER_HTTP,
  TRACKER_UDP,
  TRACKER_DHT,
};

// Just forward declare everything here so we can keep the actual
// headers clean.
class AddressList;
class AvailableList;
class Bitfield;
class Block;
class BlockFailed;
class BlockList;
class BlockTransfer;
class Chunk;
class ChunkList;
class ChunkManager;
class ChunkSelector;
class ClientInfo;
class ClientList;
class ConnectionList;
class ConnectionManager;
class DhtRouter;
class Download;
class DownloadInfo;
class DownloadMain;
class DownloadWrapper;
class FileList;
class FileManager;
class Event;
class File;
class FileList;
class Handshake;
class HandshakeManager;
class HashString;
class Listen;
class Manager;
class MemoryChunk;
class Object;
class Path;
class Peer;
class PeerConnectionBase;
class PeerInfo;
class PeerList;
class Piece;
class Poll;
class ProtocolExtension;
class Rate;
class ResourceManager;
class SocketSet;
class Throttle;
class TrackerController;
class TrackerList;
class TransferList;

class thread_interrupt;

namespace net {
class Resolver;
}

namespace tracker {
class DhtController;
class Tracker;
}

namespace utils {
class Thread;
}

// This should only need to be set when compiling libtorrent.
#ifdef SUPPORT_ATTRIBUTE_VISIBILITY
  #define LIBTORRENT_NO_EXPORT __attribute__ ((visibility("hidden")))
  #define LIBTORRENT_EXPORT __attribute__ ((visibility("default")))
#else
  #define LIBTORRENT_NO_EXPORT
  #define LIBTORRENT_EXPORT
#endif

}

#endif
