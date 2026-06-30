#ifndef LIBTORRENT_COMMON_H
#define LIBTORRENT_COMMON_H

#include <new>
#include <atomic>
#include <cerrno>
#include <cinttypes>
#include <cstddef>
#include <cstring>
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <thread>

#if defined(__has_cpp_attribute) && __has_cpp_attribute(gnu::visibility)
  #define RTORRENT_EXPORT    [[gnu::visibility("default")]]
  #define RTORRENT_NO_EXPORT [[gnu::visibility("hidden")]]
#else
  #define RTORRENT_EXPORT
  #define RTORRENT_NO_EXPORT
#endif

// This should only need to be set when compiling libtorrent.
#ifdef SUPPORT_ATTRIBUTE_VISIBILITY
  #define LIBTORRENT_NO_EXPORT __attribute__ ((visibility("hidden")))
  #define LIBTORRENT_EXPORT __attribute__ ((visibility("default")))
#else
  #define LIBTORRENT_NO_EXPORT
  #define LIBTORRENT_EXPORT
#endif

#define align_cacheline alignas(LT_SMP_CACHE_BYTES)

struct sockaddr;
struct sockaddr_in;
struct sockaddr_in6;
struct sockaddr_un;

using namespace std::chrono_literals;

namespace torrent {

namespace system {

using callback_id = std::shared_ptr<std::atomic<uint32_t>>;

} // namespace system

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
class Throttle;
class TrackerController;
class TrackerList;
class TransferList;

namespace net {

class HttpGet;
class HttpStack;
class Resolver;

} // namespace net

namespace runtime {

class NetworkConfig;
class NetworkManager;
class SocketManager;

} // namespace runtime

namespace tracker {

class DhtController;
class Manager;
class Tracker;

} // namespace tracker

namespace system {

class Poll;
class Thread;

} // namespace system

namespace utils {

class Scheduler;
class SchedulerEntry;

} // namespace utils


namespace RTORRENT_EXPORT this_thread {

system::Thread*           thread();
const char*               thread_name();
std::string               thread_name_str();
std::thread::id           thread_id();

std::chrono::microseconds cached_time();
std::chrono::seconds      cached_seconds();

system::Poll*             poll();
net::Resolver*            resolver();
utils::Scheduler*         scheduler();

} // namespace torrent::this_thread

namespace disk_thread {

system::Thread*          thread();
std::thread::id          thread_id();

} // namespace torrent::disk_thread

namespace main_thread {

system::Thread*          thread();
std::thread::id          thread_id();

void                     set_client_callback(std::function<void()> fn);

uint32_t                 hash_queue_size();

} // namespace torrent::main_thread

namespace net_thread {

system::Thread*          thread();
std::thread::id          thread_id();

// TODO: Move to runtime.
net::HttpStack*          http_stack();

} // namespace torrent::net_thread

namespace tracker_thread {

system::Thread*          thread();
std::thread::id          thread_id();

tracker::Manager*        manager();

} // namespace torrent::tracker_thread

} // namespace torrent

#endif
