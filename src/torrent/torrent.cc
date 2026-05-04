#include "config.h"

#include "torrent/torrent.h"

#include <algorithm>
#include <random>
#include <curl/curl.h>

#include "runtime.h"
#include "data/file_manager.h"
#include "data/hash_queue.h"
#include "data/thread_disk.h"
#include "download/download_constructor.h"
#include "download/download_manager.h"
#include "download/download_wrapper.h"
#include "manager.h"
#include "net/thread_net.h"
#include "protocol/handshake_manager.h"
#include "protocol/peer_factory.h"
#include "thread_main.h"
#include "torrent/connection_manager.h"
#include "torrent/download/resource_manager.h"
#include "torrent/download_info.h"
#include "torrent/exceptions.h"
#include "torrent/object.h"
#include "torrent/object_stream.h"
#include "torrent/throttle.h"
#include "torrent/peer/connection_list.h"
#include "torrent/net/http_stack.h"
#include "torrent/net/poll.h"
#include "torrent/runtime/network_manager.h"
#include "tracker/thread_tracker.h"
#include "utils/instrumentation.h"

namespace torrent {

namespace {

uint32_t
calculate_max_open_files(uint32_t open_max) {
  if (open_max >= 16384)
    return 512;
  else if (open_max >= 8096)
    return 256;
  else if (open_max >= 1024)
    return 128;
  else if (open_max >= 512)
    return 64;
  else if (open_max >= 128)
    return 16;
  else // Assumes we don't try less than 64.
    return 4;
}

uint32_t
calculate_max_http_host_connections(uint32_t open_max) {
  if (open_max >= 16384)
    return 3;
  else if (open_max >= 8096)
    return 2;
  else // Assumes we don't try less than 64.
    return 1;
}

uint32_t
calculate_max_http_total_connections(uint32_t open_max) {
  if (open_max >= 16384)
    return 128;
  else if (open_max >= 8096)
    return 64;
  else if (open_max >= 1024)
    return 32;
  else if (open_max >= 512)
    return 16;
  else if (open_max >= 128)
    return 8;
  else // Assumes we don't try less than 64.
    return 4;
}

uint32_t
calculate_reserved(uint32_t open_max) {
  if (open_max >= 16384)
    return 512;
  else if (open_max >= 8096)
    return 256;
  else if (open_max >= 1024)
    return 128;
  else if (open_max >= 512)
    return 64;
  else if (open_max >= 128)
    return 32;
  else // Assumes we don't try less than 64.
    return 16;
}

std::string
generate_random(size_t length) {
  std::random_device rd;
  std::mt19937 mt(rd());

  using bytes_randomizer = std::independent_bits_engine<std::mt19937, CHAR_BIT, uint8_t>;
  bytes_randomizer bytes(mt);

  std::string s;
  s.reserve(length);

  std::generate_n(std::back_inserter(s), length, std::ref(bytes));
  return s;
}

}

namespace system {

// TODO: Move.

const char*
errno_enum(int status) {
  // The Open Group Base Specifications Issue 6
  switch (status) {
  case 0:               return "0";
  case E2BIG:           return "E2BIG";
  case EACCES:          return "EACCES";
  case EADDRINUSE:      return "EADDRINUSE";
  case EADDRNOTAVAIL:   return "EADDRNOTAVAIL";
  case EAFNOSUPPORT:    return "EAFNOSUPPORT";
  case EAGAIN:          return "EAGAIN";
  case EALREADY:        return "EALREADY";
  case EBADF:           return "EBADF";
  case EBADMSG:         return "EBADMSG";
  case EBUSY:           return "EBUSY";
  case ECANCELED:       return "ECANCELED";
  case ECHILD:          return "ECHILD";
  case ECONNABORTED:    return "ECONNABORTED";
  case ECONNREFUSED:    return "ECONNREFUSED";
  case ECONNRESET:      return "ECONNRESET";
  case EDEADLK:         return "EDEADLK";
  case EDESTADDRREQ:    return "EDESTADDRREQ";
  case EDOM:            return "EDOM";
  case EDQUOT:          return "EDQUOT";
  case EEXIST:          return "EEXIST";
  case EFAULT:          return "EFAULT";
  case EFBIG:           return "EFBIG";
  case EHOSTUNREACH:    return "EHOSTUNREACH";
  case EIDRM:           return "EIDRM";
  case EILSEQ:          return "EILSEQ";
  case EINPROGRESS:     return "EINPROGRESS";
  case EINTR:           return "EINTR";
  case EINVAL:          return "EINVAL";
  case EIO:             return "EIO";
  case EISCONN:         return "EISCONN";
  case EISDIR:          return "EISDIR";
  case ELOOP:           return "ELOOP";
  case EMFILE:          return "EMFILE";
  case EMLINK:          return "EMLINK";
  case EMSGSIZE:        return "EMSGSIZE";
#if defined(EMULTIHOP)
  case EMULTIHOP:       return "EMULTIHOP";
#endif
  case ENAMETOOLONG:    return "ENAMETOOLONG";
  case ENETDOWN:        return "ENETDOWN";
  case ENETRESET:       return "ENETRESET";
  case ENETUNREACH:     return "ENETUNREACH";
  case ENFILE:          return "ENFILE";
  case ENOBUFS:         return "ENOBUFS";
  case ENODATA:         return "ENODATA";
  case ENODEV:          return "ENODEV";
  case ENOENT:          return "ENOENT";
  case ENOEXEC:         return "ENOEXEC";
  case ENOLCK:          return "ENOLCK";
  case ENOLINK:         return "ENOLINK";
  case ENOMEM:          return "ENOMEM";
  case ENOMSG:          return "ENOMSG";
  case ENOPROTOOPT:     return "ENOPROTOOPT";
  case ENOSPC:          return "ENOSPC";
  case ENOSR:           return "ENOSR";
  case ENOSTR:          return "ENOSTR";
  case ENOSYS:          return "ENOSYS";
  case ENOTCONN:        return "ENOTCONN";
  case ENOTDIR:         return "ENOTDIR";
  case ENOTEMPTY:       return "ENOTEMPTY";
  case ENOTSOCK:        return "ENOTSOCK";
  case ENOTTY:          return "ENOTTY";
  case ENXIO:           return "ENXIO";
  case EOPNOTSUPP:      return "EOPNOTSUPP";
  case EOVERFLOW:       return "EOVERFLOW";
  case EPERM:           return "EPERM";
  case EPIPE:           return "EPIPE";
  case EPROTO:          return "EPROTO";
  case EPROTONOSUPPORT: return "EPROTONOSUPPORT";
  case EPROTOTYPE:      return "EPROTOTYPE";
  case ERANGE:          return "ERANGE";
  case EROFS:           return "EROFS";
  case ESPIPE:          return "ESPIPE";
  case ESRCH:           return "ESRCH";
  case ESTALE:          return "ESTALE";
  case ETIME:           return "ETIME";
  case ETIMEDOUT:       return "ETIMEDOUT";
  case ETXTBSY:         return "ETXTBSY";
  case EXDEV:           return "EXDEV";

  default:
    // Handle potentially duplicate error numbers here.
    switch (status) {
    case ENOTSUP:       return "ENOTSUP";
    case EWOULDBLOCK:   return "EWOULDBLOCK";
    default:
      static thread_local char buffer[16];
      std::snprintf(buffer, sizeof(buffer), "E%d", status);

      return buffer;
    };
  };
}

std::string
errno_enum_str(int status) {
  return errno_enum(status);
}

} // namespace torrent::system

void
initialize_main_thread() {
  ThreadMain::create_thread();
  ThreadMain::thread_main()->init_thread();
}

void
initialize() {
  if (manager != nullptr)
    throw internal_error("torrent::initialize(...) called but the library has already been initialized");

  instrumentation_initialize();
  curl_global_init(CURL_GLOBAL_ALL);

  Runtime::initialize(torrent::this_thread::thread());

  manager = new Manager;

  ThreadDisk::create_thread();
  ThreadNet::create_thread();
  ThreadTracker::create_thread(ThreadMain::thread_main());

  auto max_open = this_thread::poll()->open_max();
  auto max_files = calculate_max_open_files(max_open);
  auto max_http_connections = calculate_max_http_total_connections(max_open);
  auto reserved = calculate_reserved(max_open);

  manager->connection_manager()->set_max_size(max_open - max_files - max_http_connections - reserved);
  manager->file_manager()->set_max_open_files(max_files);

  net_thread::http_stack()->set_max_host_connections(calculate_max_http_host_connections(max_open));
  net_thread::http_stack()->set_max_total_connections(max_http_connections);

  disk_thread::thread()->init_thread();
  net_thread::thread()->init_thread();
  tracker_thread::thread()->init_thread();

  disk_thread::thread()->start_thread();
  net_thread::thread()->start_thread();
  tracker_thread::thread()->start_thread();
}

// Clean up and close stuff. Stopping all torrents and waiting for
// them to finish is not required, but recommended.
void
cleanup() {
  if (manager == nullptr)
    throw internal_error("torrent::cleanup() called but the library is not initialized.");

  // Might need to wait for the threads to finish?
  runtime::network_manager()->cleanup();

  tracker_thread::thread()->stop_thread_wait();
  disk_thread::thread()->stop_thread_wait();
  net_thread::thread()->stop_thread_wait();

  ThreadTracker::destroy_thread();
  ThreadDisk::destroy_thread();
  ThreadNet::destroy_thread();

  manager->cleanup();
  Runtime::cleanup();

  delete manager;
  manager = nullptr;

  curl_global_cleanup();
}

bool
is_initialized() {
  return manager != NULL;
}

bool
is_inactive() {
  return manager == nullptr || std::all_of(manager->download_manager()->begin(), manager->download_manager()->end(), std::mem_fn(&DownloadWrapper::is_stopped));
}

void
set_main_thread_slots(std::function<void()> do_work) {
  ThreadMain::thread_main()->slot_do_work() = std::move(do_work);
}

ChunkManager*      chunk_manager()       { return manager->chunk_manager(); }
ClientList*        client_list()         { return manager->client_list(); }
ConnectionManager* connection_manager()  { return manager->connection_manager(); }
FileManager*       file_manager()        { return manager->file_manager(); }
ResourceManager*   resource_manager()    { return manager->resource_manager(); }

uint32_t
total_handshakes() {
  return manager->handshake_manager()->size();
}

Throttle* down_throttle_global() { return manager->download_throttle(); }
Throttle* up_throttle_global() { return manager->upload_throttle(); }

const Rate* down_rate() { return manager->download_throttle()->rate(); }
const Rate* up_rate() { return manager->upload_throttle()->rate(); }
const char* version() { return VERSION; }

EncodingList*
encoding_list() {
  return manager->encoding_list();
}

Download
download_add(Object* object, uint32_t tracker_key) {
  auto download = std::make_unique<DownloadWrapper>();

  DownloadConstructor ctor;
  ctor.set_download(download.get());
  ctor.set_encoding_list(manager->encoding_list());

  ctor.initialize(*object);

  std::string infoHash;
  if (download->info()->is_meta_download())
    infoHash = object->get_key("info").get_key("pieces").as_string();
  else
    infoHash = object_sha1(&object->get_key("info"));

  if (manager->download_manager()->find(infoHash) != manager->download_manager()->end())
    throw input_error("Info hash already used by another torrent.");

  if (!download->info()->is_meta_download()) {
    char buffer[1024];
    uint64_t metadata_size = 0;
    object_write_bencode_c(&object_write_to_size, &metadata_size, object_buffer_t(buffer, buffer + sizeof(buffer)), &object->get_key("info"));
    download->main()->set_metadata_size(metadata_size);
  }

  std::string local_id = PEER_NAME + generate_random(20 - std::string(PEER_NAME).size());

  download->set_hash_queue(ThreadMain::thread_main()->hash_queue());
  download->initialize(infoHash, local_id, tracker_key);

  // Add trackers, etc, after setting the info hash so that log
  // entries look sane.
  ctor.parse_tracker(*object);

  // Default PeerConnection factory functions.
  download->main()->connection_list()->slot_new_connection(&createPeerConnectionDefault);

  // Consider move as much as possible into this function
  // call. Anything that won't cause possible torrent creation errors
  // go in there.
  manager->initialize_download(download.get());

  download->set_bencode(object);
  return Download(download.release());
}

void
download_remove(Download d) {
  manager->cleanup_download(d.ptr());
}

// Add all downloads to dlist. Make sure it's cleared.
void
download_list(DList& dlist) {
  dlist.insert(dlist.end(), manager->download_manager()->begin(), manager->download_manager()->end());
}

// Make sure you check that it's valid.
Download
download_find(const std::string& infohash) {
  return *manager->download_manager()->find(infohash);
}

uint32_t
download_priority(Download d) {
  auto itr = manager->resource_manager()->find(d.ptr()->main());

  if (itr == manager->resource_manager()->end())
    throw internal_error("torrent::download_priority(...) could not find the download in the resource manager.");

  return itr->priority();
}

// TODO: Remove this.
void
download_set_priority(Download d, uint32_t pri) {
  auto itr = manager->resource_manager()->find(d.ptr()->main());

  if (itr == manager->resource_manager()->end())
    throw internal_error("torrent::download_set_priority(...) could not find the download in the resource manager.");

  if (pri > 1024)
    throw internal_error("torrent::download_set_priority(...) received an invalid priority.");

  ResourceManager::set_priority(itr, pri);
}

} // namespace torrent
