#ifndef LIBTORRENT_DOWNLOAD_CHUNK_SELECTOR_H
#define LIBTORRENT_DOWNLOAD_CHUNK_SELECTOR_H

#include <cinttypes>

#include "torrent/bitfield.h"
#include "torrent/data/download_data.h"
#include "torrent/utils/ranges.h"
#include "utils/partial_queue.h"

namespace torrent {

// This class is responsible for deciding on which chunk index to
// download next based on the peer's bitfield. It keeps its own
// bitfield which starts out as a copy of Content::bitfield but sets
// chunks that are being downloaded.
//
// When updating Content::bitfield, make sure you update this bitfield
// and unmark any chunks in Delegator.

class ChunkStatistics;
class PeerChunks;

class ChunkSelector {
public:
  struct find_result {
    uint32_t index;
    bool     high;
  };

  static constexpr auto invalid_chunk = ~uint32_t{0};

  // Best-effort ratio of high to normal priority chunks picked by find(), so high priority does
  // not starve the rest of the torrent.
  struct priority_ratio {
    uint32_t high;
    uint32_t normal;

    bool operator==(const priority_ratio&) const = default;
  };

  static constexpr priority_ratio default_ratio{3, 1};

  // The ratio used while a priority window is set, from 1:5 when nothing is completed down to
  // 1:1 once 80% of the torrent is, in 20% steps.
  static priority_ratio window_ratio(uint32_t completed_chunks, uint32_t size_chunks);

  ChunkSelector(download_data* data) : m_data(data) {}

  bool                empty() const                 { return size() == 0; }
  uint32_t            size() const                  { return m_data->untouched_bitfield()->size_bits(); }

  //  const Bitfield*     bitfield()                    { return m_data->untouched_bitfield(); }

  // priority_ranges*    high_priority()               { return &m_highPriority; }
  // priority_ranges*    normal_priority()             { return &m_normalPriority; }

  // Initialize doesn't update the priority cache, so it is as if it
  // has empty priority ranges.
  void                initialize(ChunkStatistics* cs);
  void                cleanup();

  // Call this once you've modified the bitfield or priorities to
  // update cached information. This must be called once before using
  // find.
  void                update_priorities();

  find_result         find(PeerChunks* pc);

  priority_ratio      ratio() const;

  // Uses window_ratio() instead of default_ratio, while a priority window is set.
  bool                has_window_ratio() const      { return m_window_ratio; }
  void                set_window_ratio(bool state)  { m_window_ratio = state; }

  // Call this to set the index as being downloaded, finished etc,
  // thus ignored. Propably should find a better name for this.
  void                using_index(uint32_t index);
  void                not_using_index(uint32_t index);

  // Moves the cursor to index, then to the next wanted chunk (high priority first, wrapping).
  // find() still re-randomizes it about once every 64 picks: a priority window comes first
  // because it is high priority, not because of the cursor.
  void                set_position(uint32_t index);

  // The caller must ensure that the chunk index is valid and has not
  // been set already.
  //
  // The user only needs to call this when it needs to know whetever
  // it should become interested, or if it is in the process of
  // downloading.
  //
  // Returns whetever we're interested in that piece.
  bool                received_have_chunk(PeerChunks* pc, uint32_t index);

private:
  bool                search_linear(const Bitfield* bf, utils::PartialQueue* pq, const download_data::priority_ranges* ranges, bool high_priority, uint32_t first, uint32_t last);
  inline bool         search_linear_range(const Bitfield* bf, utils::PartialQueue* pq, bool high_priority, uint32_t first, uint32_t last);
  inline bool         search_linear_byte(utils::PartialQueue* pq, bool high_priority, uint32_t index, Bitfield::value_type wanted);

//   inline uint32_t     search_rarest(const Bitfield* bf, priority_ranges* ranges, uint32_t first, uint32_t last);
//   inline uint32_t     search_rarest_range(const Bitfield* bf, uint32_t first, uint32_t last);
//   inline uint32_t     search_rarest_byte(uint8_t wanted);

  void                advance_position();
  void                add_to_balance(bool high_priority);

  download_data*      m_data;

  ChunkStatistics*    m_statistics;

  utils::PartialQueue m_shared_queue;

  uint32_t            m_position;
  int64_t             m_high_selection_balance{};
  bool                m_window_ratio{false};
};

} // namespace torrent

#endif // LIBTORRENT_DOWNLOAD_CHUNK_SELECTOR_H
