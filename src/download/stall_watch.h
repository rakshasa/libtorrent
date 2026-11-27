#ifndef LIBTORRENT_DOWNLOAD_STALL_WATCH_H
#define LIBTORRENT_DOWNLOAD_STALL_WATCH_H

#include <chrono>
#include <cstdint>

#include "torrent/rate.h"

namespace torrent {

// A download's own rate, looked at once a tick (30 seconds). A tick is starved when it brought less than 2 KiB/s, or
// less than a tenth of the download's average rate over the last ten minutes; the download is stalled after two
// starved ticks in a row. The ten minutes are a Rate of KiB, so a tick of any size fits Rate::insert().
class StallWatch {
public:
  static constexpr uint32_t history_seconds = 10 * 60;
  static constexpr uint32_t tick_seconds    = 30;
  static constexpr uint64_t floor_rate      = 2 * 1024;
  static constexpr uint64_t ratio           = 10;
  static constexpr uint32_t stalled_after   = 2;

  // total: the download's whole received count. Returns whether the download is stalled.
  bool update(uint64_t total);
  bool is_stalled() const { return m_starved >= stalled_after; }

private:
  Rate                 m_history{ history_seconds, history_seconds, tick_seconds, history_seconds };

  std::chrono::seconds m_last_update{};
  uint64_t             m_last_total{};
  uint32_t             m_starved{};
};

} // namespace torrent

#endif
