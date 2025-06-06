#ifndef LIBTORRENT_DATA_DOWNLOAD_DATA_H
#define LIBTORRENT_DATA_DOWNLOAD_DATA_H

#include <functional>

#include <torrent/common.h>
#include <torrent/bitfield.h>
#include <torrent/hash_string.h>
#include <torrent/utils/ranges.h>

namespace torrent {
class ChunkListNode;
class ChunkSelector;
class Download;
class DownloadWrapper;
class FileList;

class download_data {
public:
  using priority_ranges = ranges<uint32_t>;

  using slot_void = std::function<void()>;

  using slot_chunk_list_node_p = std::function<void(ChunkListNode*)>;

  const HashString&      hash() const                  { return m_hash; }

  bool                   is_partially_done() const     { return m_wanted_chunks == 0; }
  bool                   is_not_partially_done() const { return m_wanted_chunks != 0; }

  const Bitfield*        completed_bitfield() const    { return &m_completed_bitfield; }
  const Bitfield*        untouched_bitfield() const    { return &m_untouched_bitfield; }

  const priority_ranges* high_priority() const         { return &m_high_priority; }
  const priority_ranges* normal_priority() const       { return &m_normal_priority; }

  uint32_t               wanted_chunks() const         { return m_wanted_chunks; }

  uint32_t               calc_wanted_chunks() const;
  void                   verify_wanted_chunks(const char* where) const;

  slot_void&             slot_initial_hash() const        { return m_slot_initial_hash; }
  slot_void&             slot_download_done() const       { return m_slot_download_done; }
  slot_void&             slot_partially_done() const      { return m_slot_partially_done; }
  slot_void&             slot_partially_restarted() const { return m_slot_partially_restarted; }
  slot_chunk_list_node_p& slot_chunk_done() const          {return m_slot_chunk_done;}

protected:
  friend class ChunkList;
  friend class ChunkSelector;
  friend class Download;
  friend class DownloadWrapper;
  friend class FileList;

  HashString&            mutable_hash()                { return m_hash; }

  Bitfield*              mutable_completed_bitfield()  { return &m_completed_bitfield; }
  Bitfield*              mutable_untouched_bitfield()  { return &m_untouched_bitfield; }

  priority_ranges*       mutable_high_priority()       { return &m_high_priority; }
  priority_ranges*       mutable_normal_priority()     { return &m_normal_priority; }

  void                   update_wanted_chunks()        { m_wanted_chunks = calc_wanted_chunks(); }
  void                   set_wanted_chunks(uint32_t n) { m_wanted_chunks = n; }

  void                   call_download_done()          { if (m_slot_download_done) m_slot_download_done(); }
  void                   call_partially_done()         { if (m_slot_partially_done) m_slot_partially_done(); }
  void                   call_partially_restarted()    { if (m_slot_partially_restarted) m_slot_partially_restarted(); }
  void                   call_chunk_done(ChunkListNode* chunk_ptr) {if(m_slot_chunk_done) m_slot_chunk_done(chunk_ptr);}
private:
  HashString             m_hash;

  Bitfield               m_completed_bitfield;
  Bitfield               m_untouched_bitfield;

  priority_ranges        m_high_priority;
  priority_ranges        m_normal_priority;

  uint32_t               m_wanted_chunks{0};

  mutable slot_void      m_slot_initial_hash;
  mutable slot_void      m_slot_download_done;
  mutable slot_void      m_slot_partially_done;
  mutable slot_void      m_slot_partially_restarted;
  mutable slot_chunk_list_node_p m_slot_chunk_done;
};

} // namespace torrent

#endif // LIBTORRENT_DATA_DOWNLOAD_DATA_H
