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

typedef enum {
  TRACKER_NONE,
  TRACKER_HTTP,
  TRACKER_UDP,
  TRACKER_DHT,
} tracker_enum;

typedef priority_enum priority_t;

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
class DhtManager;
class DhtRouter;
class Download;
class DownloadInfo;
class DownloadMain;
class DownloadWrapper;
class FileList;
class Event;
class File;
class FileList;
class Handshake;
class HandshakeManager;
class HashString;
class Listen;
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
class SocketSet;
class Throttle;
class Tracker;
class TrackerController;
class TrackerList;
class TransferList;

namespace tracker {

class TrackerManager;

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
