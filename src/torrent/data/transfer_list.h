#ifndef LIBTORRENT_TRANSFER_LIST_H
#define LIBTORRENT_TRANSFER_LIST_H

#include <functional>
#include <vector>
#include <torrent/common.h>

namespace torrent {

class LIBTORRENT_EXPORT TransferList : public std::vector<BlockList*> {
public:
  using base_type           = std::vector<BlockList*>;
  using completed_list_type = std::vector<std::pair<int64_t, uint32_t>>;

  using base_type::value_type;
  using base_type::reference;
  using base_type::difference_type;

  using base_type::iterator;
  using base_type::const_iterator;
  using base_type::reverse_iterator;
  using base_type::const_reverse_iterator;
  using base_type::size;
  using base_type::empty;

  using base_type::begin;
  using base_type::end;
  using base_type::rbegin;
  using base_type::rend;

  TransferList() = default;
  ~TransferList();
  TransferList(const TransferList&) = delete;
  TransferList& operator=(const TransferList&) = delete;

  iterator            find(uint32_t index);
  const_iterator      find(uint32_t index) const;

  const completed_list_type& completed_list() const { return m_completedList; }

  uint32_t            succeeded_count() const { return m_succeededCount; }
  uint32_t            failed_count() const { return m_failedCount; }

  //
  // Internal to libTorrent:
  //

  void                clear();

  iterator            insert(const Piece& piece, uint32_t blockSize);
  iterator            erase(iterator itr);

  void                finished(BlockTransfer* transfer);

  void                hash_succeeded(uint32_t index, Chunk* chunk);
  void                hash_failed(uint32_t index, Chunk* chunk);

  using slot_chunk_index = std::function<void(uint32_t)>;
  using slot_peer_info   = std::function<void(PeerInfo*)>;

  slot_chunk_index&   slot_canceled()  { return m_slot_canceled; }
  slot_chunk_index&   slot_completed() { return m_slot_completed; }
  slot_chunk_index&   slot_queued()    { return m_slot_queued; }
  slot_peer_info&     slot_corrupt()   { return m_slot_corrupt; }

private:
  static unsigned int update_failed(BlockList* blockList, Chunk* chunk);
  void                mark_failed_peers(BlockList* blockList, Chunk* chunk);

  void                retry_most_popular(BlockList* blockList, Chunk* chunk);

  slot_chunk_index    m_slot_canceled;
  slot_chunk_index    m_slot_completed;
  slot_chunk_index    m_slot_queued;
  slot_peer_info      m_slot_corrupt;

  completed_list_type m_completedList;

  uint32_t            m_succeededCount{0};
  uint32_t            m_failedCount{0};
};

} // namespace torrent

#endif
