#include "config.h"

#include "torrent/torrent.h"

#include "manager.h"
#include "data/file_manager.h"
#include "data/hash_queue.h"
#include "data/hash_torrent.h"
#include "data/thread_disk.h"
#include "download/download_constructor.h"
#include "download/download_manager.h"
#include "download/download_wrapper.h"
#include "protocol/handshake_manager.h"
#include "protocol/peer_factory.h"
#include "rak/address_info.h"
#include "rak/string_manip.h"
#include "torrent/connection_manager.h"
#include "torrent/exceptions.h"
#include "torrent/object.h"
#include "torrent/object_stream.h"
#include "torrent/throttle.h"
#include "torrent/poll.h"
#include "torrent/peer/connection_list.h"
#include "torrent/download/resource_manager.h"
#include "tracker/thread_tracker.h"
#include "utils/instrumentation.h"

namespace torrent {

uint32_t
calculate_max_open_files(uint32_t openMax) {
  if (openMax >= 8096)
    return 256;
  else if (openMax >= 1024)
    return 128;
  else if (openMax >= 512)
    return 64;
  else if (openMax >= 128)
    return 16;
  else // Assumes we don't try less than 64.
    return 4;
}

uint32_t
calculate_reserved(uint32_t openMax) {
  if (openMax >= 8096)
    return 256;
  else if (openMax >= 1024)
    return 128;
  else if (openMax >= 512)
    return 64;
  else if (openMax >= 128)
    return 32;
  else // Assumes we don't try less than 64.
    return 16;
}

void
initialize() {
  if (manager != NULL)
    throw internal_error("torrent::initialize(...) called but the library has already been initialized");

  cachedTime = rak::timer::current();

  instrumentation_initialize();

  manager = new Manager;
  thread_disk = new ThreadDisk;
  thread_tracker = new ThreadTracker;

  manager->thread_main()->init_thread();

  uint32_t maxFiles = calculate_max_open_files(manager->poll()->open_max());

  manager->connection_manager()->set_max_size(manager->poll()->open_max() - maxFiles - calculate_reserved(manager->poll()->open_max()));
  manager->file_manager()->set_max_open_files(maxFiles);

  thread_disk->init_thread();
  thread_tracker->init_thread();

  thread_disk->start_thread();
  thread_tracker->start_thread();
}

// Clean up and close stuff. Stopping all torrents and waiting for
// them to finish is not required, but recommended.
void
cleanup() {
  if (manager == NULL)
    throw internal_error("torrent::cleanup() called but the library is not initialized.");

  thread_tracker->stop_thread_wait();
  thread_disk->stop_thread_wait();

  delete manager;
  manager = NULL;
}

bool
is_initialized() {
  return manager != NULL;
}

bool
is_inactive() {
  return manager == nullptr || std::all_of(manager->download_manager()->begin(), manager->download_manager()->end(), std::mem_fn(&DownloadWrapper::is_stopped));
}

thread_base*
main_thread() {
  return manager->thread_main();
}

ChunkManager*      chunk_manager() { return manager->chunk_manager(); }
ClientList*        client_list() { return manager->client_list(); }
ConnectionManager* connection_manager() { return manager->connection_manager(); }
FileManager*       file_manager() { return manager->file_manager(); }
ResourceManager*   resource_manager() { return manager->resource_manager(); }

tracker::DhtController* dht_controller() { return manager->dht_controller(); }

uint32_t
total_handshakes() {
  return manager->handshake_manager()->size();
}

Throttle* down_throttle_global() { return manager->download_throttle(); }
Throttle* up_throttle_global() { return manager->upload_throttle(); }

const Rate* down_rate() { return manager->download_throttle()->rate(); }
const Rate* up_rate() { return manager->upload_throttle()->rate(); }
const char* version() { return VERSION; }

uint32_t hash_queue_size() { return manager->hash_queue()->size(); }

EncodingList*
encoding_list() {
  return manager->encoding_list();
}

Download
download_add(Object* object, uint32_t tracker_key) {
  auto                download = std::make_unique<DownloadWrapper>();

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

  std::string local_id = PEER_NAME + rak::generate_random<std::string>(20 - std::string(PEER_NAME).size());

  download->set_hash_queue(manager->hash_queue());
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
  for (auto download : *manager->download_manager())
    dlist.emplace_back(download);
}

// Make sure you check that it's valid.
Download
download_find(const std::string& infohash) {
  return *manager->download_manager()->find(infohash);
}

uint32_t
download_priority(Download d) {
  ResourceManager::iterator itr = manager->resource_manager()->find(d.ptr()->main());

  if (itr == manager->resource_manager()->end())
    throw internal_error("torrent::download_priority(...) could not find the download in the resource manager.");

  return itr->priority();
}

// TODO: Remove this.
void
download_set_priority(Download d, uint32_t pri) {
  ResourceManager::iterator itr = manager->resource_manager()->find(d.ptr()->main());

  if (itr == manager->resource_manager()->end())
    throw internal_error("torrent::download_set_priority(...) could not find the download in the resource manager.");

  if (pri > 1024)
    throw internal_error("torrent::download_set_priority(...) received an invalid priority.");

  manager->resource_manager()->set_priority(itr, pri);
}

}
