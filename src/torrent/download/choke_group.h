#ifndef LIBTORRENT_DOWNLOAD_CHOKE_GROUP_H
#define LIBTORRENT_DOWNLOAD_CHOKE_GROUP_H

#include <string>
#include <vector>
#include <cinttypes>
#include <torrent/common.h>
#include <torrent/download/choke_queue.h>

// TODO: Separate out resource_manager_entry.
#include <torrent/download/resource_manager.h>

namespace torrent {

class choke_queue;
class resource_manager_entry;

class LIBTORRENT_EXPORT choke_group {
public:
  enum tracker_mode_enum {
    TRACKER_MODE_NORMAL,
    TRACKER_MODE_AGGRESSIVE
  };

  const std::string&  name() const { return m_name; }
  void                set_name(const std::string& name) { m_name = name; }

  tracker_mode_enum   tracker_mode() const                   { return m_tracker_mode; }
  void                set_tracker_mode(tracker_mode_enum tm) { m_tracker_mode = tm; }

  choke_queue*        up_queue()   { return &m_up_queue; }
  choke_queue*        down_queue() { return &m_down_queue; }

  const choke_queue*  c_up_queue() const   { return &m_up_queue; }
  const choke_queue*  c_down_queue() const { return &m_down_queue; }

  uint32_t            up_requested() const   { return std::min(m_up_queue.size_total(), m_up_queue.max_unchoked()); }
  uint32_t            down_requested() const { return std::min(m_down_queue.size_total(), m_down_queue.max_unchoked()); }

  bool                empty() const { return m_first == m_last; }
  uint32_t            size() const { return std::distance(m_first, m_last); }

  uint64_t            up_rate() const;
  uint64_t            down_rate() const;

  unsigned int        up_unchoked() const   { return m_up_queue.size_unchoked(); }
  unsigned int        down_unchoked() const { return m_down_queue.size_unchoked(); }

  // Internal:

  resource_manager_entry* first() { return m_first; }
  resource_manager_entry* last()  { return m_last; }

  void                    set_first(resource_manager_entry* first) { m_first = first; }
  void                    set_last(resource_manager_entry* last)   { m_last = last; }

  void                    inc_iterators() { m_first++; m_last++; }
  void                    dec_iterators() { m_first--; m_last--; }

private:
  std::string             m_name;
  tracker_mode_enum       m_tracker_mode{TRACKER_MODE_NORMAL};

  choke_queue             m_up_queue;
  choke_queue             m_down_queue{choke_queue::flag_unchoke_all_new};

  resource_manager_entry* m_first{};
  resource_manager_entry* m_last{};
};

} // namespace torrent

#endif
