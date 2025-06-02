#ifndef LIBTORRENT_DATA_HASH_CHECK_QUEUE_H
#define LIBTORRENT_DATA_HASH_CHECK_QUEUE_H

#include <deque>
#include <functional>
#include <mutex>

// TODO: Create separate directory for thread_disk's hash checking code.

namespace torrent {

class HashString;
class HashChunk;

class HashCheckQueue : private std::deque<HashChunk*> {
public:
  using base_type         = std::deque<HashChunk*>;
  using slot_chunk_handle = std::function<void(HashChunk*, const HashString&)>;

  using base_type::iterator;

  using base_type::empty;
  using base_type::size;

  using base_type::begin;
  using base_type::end;
  using base_type::front;
  using base_type::back;

  HashCheckQueue();
  ~HashCheckQueue();

  // Guarded functions for adding new...

  void                push_back(HashChunk* node);
  void                perform();

  bool                remove(HashChunk* node);

  slot_chunk_handle&  slot_chunk_done() { return m_slot_chunk_done; }

private:
  std::mutex          m_lock;
  slot_chunk_handle   m_slot_chunk_done;
};

} // namespace torrent

#endif
