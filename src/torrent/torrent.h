#ifndef LIBTORRENT_TORRENT_H
#define LIBTORRENT_TORRENT_H

#include <list>
#include <string>
#include <torrent/common.h>
#include <torrent/download.h>

namespace torrent {

// Make sure you seed srandom and srand48 if available.
void                initialize_main_thread()LIBTORRENT_EXPORT;
void                initialize() LIBTORRENT_EXPORT;

// Clean up and close stuff. Stopping all torrents and waiting for
// them to finish is not required, but recommended.
void                cleanup() LIBTORRENT_EXPORT;

bool                is_inactive() LIBTORRENT_EXPORT;
bool                is_initialized() LIBTORRENT_EXPORT;

void                set_main_thread_slots(std::function<void()> do_work) LIBTORRENT_EXPORT;

ChunkManager*       chunk_manager() LIBTORRENT_EXPORT;
ClientList*         client_list() LIBTORRENT_EXPORT;
ConnectionManager*  connection_manager() LIBTORRENT_EXPORT;
FileManager*        file_manager() LIBTORRENT_EXPORT;
ResourceManager*    resource_manager() LIBTORRENT_EXPORT;

uint32_t            total_handshakes() LIBTORRENT_EXPORT;

Throttle*           down_throttle_global() LIBTORRENT_EXPORT;
Throttle*           up_throttle_global() LIBTORRENT_EXPORT;

const Rate*         down_rate() LIBTORRENT_EXPORT;
const Rate*         up_rate() LIBTORRENT_EXPORT;

const char*         version() LIBTORRENT_EXPORT;

using DList        = std::list<Download>;
using EncodingList = std::list<std::string>;

EncodingList*       encoding_list() LIBTORRENT_EXPORT;

// Will always return a valid Download. On errors it
// throws. 'encodingList' contains a list of prefered encodings to use
// for file names.
//
// The Object must be on the heap allocated with 'new'. If
// 'download_add' throws the client must handle the deletion, else it
// is done by 'download_remove'.
//
// Might consider redesigning that...
Download            download_add(Object* s, uint32_t tracker_key) LIBTORRENT_EXPORT;
void                download_remove(Download d) LIBTORRENT_EXPORT;

// Add all downloads to dlist. The client is responsible for clearing
// it before the call.
void                download_list(DList& dlist) LIBTORRENT_EXPORT;

// Make sure you check the returned Download's is_valid().
Download            download_find(const std::string& infohash) LIBTORRENT_EXPORT;

uint32_t            download_priority(Download d) LIBTORRENT_EXPORT;
void                download_set_priority(Download d, uint32_t pri) LIBTORRENT_EXPORT;

} // namespace torrent

#endif
