#ifndef LIBTORRENT_TORRENT_SYSTEM_COMMON_H
#define LIBTORRENT_TORRENT_SYSTEM_COMMON_H

#include <atomic>
#include <chrono>
#include <cinttypes>
#include <cstdint>
#include <memory>


using namespace std::chrono_literals;


namespace torrent::system {

class Event;
class Poll;
class Scheduler;
class SchedulerEntry;
class Thread;

using callback_id    = std::shared_ptr<std::atomic<uint32_t>>;

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
