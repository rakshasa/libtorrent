#ifndef LIBTORRENT_COMMON_H
#define LIBTORRENT_COMMON_H

#include <cinttypes>
#include <cstddef>
#include <chrono>
#include <functional>
#include <thread>

struct sockaddr;
struct sockaddr_in;
struct sockaddr_in6;
struct sockaddr_un;

using namespace std::chrono_literals;

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
class MemoryChunk;
class Object;
class Path;
class Peer;
class PeerConnectionBase;
class PeerInfo;
class PeerList;
class Piece;
class ProtocolExtension;
class Rate;
class ResourceManager;
class SocketSet;
class Throttle;
class TrackerController;
class TrackerList;
class TransferList;

namespace net {

class HttpGet;
class HttpStack;
class NetworkConfig;
class NetworkManager;
class Poll;
class Resolver;

} // namespace net

namespace tracker {

class DhtController;
class Tracker;

} // namespace tracker

namespace utils {

class Scheduler;
class SchedulerEntry;
class Thread;

} // namespace utils

} // namespace torrent

// This should only need to be set when compiling libtorrent.
#ifdef SUPPORT_ATTRIBUTE_VISIBILITY
  #define LIBTORRENT_NO_EXPORT __attribute__ ((visibility("hidden")))
  #define LIBTORRENT_EXPORT __attribute__ ((visibility("default")))
#else
  #define LIBTORRENT_NO_EXPORT
  #define LIBTORRENT_EXPORT
#endif

namespace torrent::config {

torrent::net::NetworkConfig* network_config() LIBTORRENT_EXPORT;

} // namespace torrent::config

namespace torrent::runtime {

torrent::net::NetworkManager* network_manager() LIBTORRENT_EXPORT;

} // namespace torrent::runtime

namespace torrent::this_thread {

torrent::utils::Thread*   thread() LIBTORRENT_EXPORT;
std::thread::id           thread_id() LIBTORRENT_EXPORT;

std::chrono::microseconds cached_time() LIBTORRENT_EXPORT;
std::chrono::seconds      cached_seconds() LIBTORRENT_EXPORT;

void                      callback(void* target, std::function<void ()>&& fn);
void                      cancel_callback(void* target);
void                      cancel_callback_and_wait(void* target);

net::Poll*                poll() LIBTORRENT_EXPORT;
net::Resolver*            resolver() LIBTORRENT_EXPORT;
utils::Scheduler*         scheduler() LIBTORRENT_EXPORT;

[[gnu::weak]] void event_open(Event* event) LIBTORRENT_EXPORT;
[[gnu::weak]] void event_open_and_count(Event* event) LIBTORRENT_EXPORT;
[[gnu::weak]] void event_close_and_count(Event* event) LIBTORRENT_EXPORT;
[[gnu::weak]] void event_closed_and_count(Event* event) LIBTORRENT_EXPORT;
[[gnu::weak]] void event_insert_read(Event* event) LIBTORRENT_EXPORT;
[[gnu::weak]] void event_insert_write(Event* event) LIBTORRENT_EXPORT;
[[gnu::weak]] void event_insert_error(Event* event) LIBTORRENT_EXPORT;
[[gnu::weak]] void event_remove_read(Event* event) LIBTORRENT_EXPORT;
[[gnu::weak]] void event_remove_write(Event* event) LIBTORRENT_EXPORT;
[[gnu::weak]] void event_remove_error(Event* event) LIBTORRENT_EXPORT;
[[gnu::weak]] void event_remove_and_close(Event* event) LIBTORRENT_EXPORT;

} // namespace torrent::this_thread

namespace torrent::main_thread {

torrent::utils::Thread* thread() LIBTORRENT_EXPORT;
std::thread::id         thread_id() LIBTORRENT_EXPORT;

void                    callback(void* target, std::function<void ()>&& fn);
void                    cancel_callback(void* target);
void                    cancel_callback_and_wait(void* target);

uint32_t                hash_queue_size() LIBTORRENT_EXPORT;

} // namespace torrent::main_thread

namespace torrent::net_thread {

torrent::utils::Thread* thread() LIBTORRENT_EXPORT;
std::thread::id         thread_id() LIBTORRENT_EXPORT;

void                    callback(void* target, std::function<void ()>&& fn);
void                    cancel_callback(void* target);
void                    cancel_callback_and_wait(void* target);

torrent::net::HttpStack* http_stack() LIBTORRENT_EXPORT;

} // namespace torrent::net_thread

#endif
