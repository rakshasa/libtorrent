#ifndef LIBTORRENT_TORRENT_H
#define LIBTORRENT_TORRENT_H

#include <list>
#include <string>
#include <torrent/common.h>
#include <torrent/download.h>

namespace RTORRENT_EXPORT torrent {

// Make sure you seed srandom and srand48 if available.
void                initialize_main_thread();
void                initialize();

// Clean up and close stuff. Stopping all torrents and waiting for
// them to finish is not required, but recommended.
void                cleanup();

ChunkManager*       chunk_manager();
ClientList*         client_list();
FileManager*        file_manager();
ResourceManager*    resource_manager();

// TODO: Move to main_thread?
Throttle*           down_throttle_global();
Throttle*           up_throttle_global();

const Rate*         down_rate();
const Rate*         up_rate();

using DList = std::list<Download>;

// Will always return a valid Download. On errors it
// throws.
//
// The Object must be on the heap allocated with 'new'. If
// 'download_add' throws the client must handle the deletion, else it
// is done by 'download_remove'.
//
// Might consider redesigning that...
Download            download_add(Object* s, uint32_t tracker_key);
void                download_remove(Download d);

// Add all downloads to dlist. The client is responsible for clearing
// it before the call.
void                download_list(DList& dlist);

// Make sure you check the returned Download's is_valid().
Download            download_find(const std::string& infohash);

uint32_t            download_priority(Download d);
void                download_set_priority(Download d, uint32_t pri);

} // namespace torrent

#endif
