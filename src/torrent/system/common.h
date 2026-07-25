#ifndef LIBTORRENT_TORRENT_SYSTEM_COMMON_H
#define LIBTORRENT_TORRENT_SYSTEM_COMMON_H

#include <chrono>
#include <cinttypes>
#include <memory>


using namespace std::chrono_literals;

// TODO: Add std::chrono::seconds, etc.


namespace torrent::system {

namespace ipc {

class Router;

} // namespace ipc

using callback_id    = std::shared_ptr<std::atomic<uint32_t>>;
using ipc_router_ptr = std::unique_ptr<ipc::Router>;


} // namespace torrent::system


// This should only need to be set when compiling libtorrent.
#ifdef SUPPORT_ATTRIBUTE_VISIBILITY
  #define LIBTORRENT_NO_EXPORT __attribute__ ((visibility("hidden")))
  #define LIBTORRENT_EXPORT __attribute__ ((visibility("default")))
#else
  #define LIBTORRENT_NO_EXPORT
  #define LIBTORRENT_EXPORT
#endif

#define align_cacheline alignas(LT_SMP_CACHE_BYTES)


#endif
