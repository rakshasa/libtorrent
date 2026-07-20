#ifndef LIBTORRENT_DOWNLOAD_CHOKE_STATUS_H
#define LIBTORRENT_DOWNLOAD_CHOKE_STATUS_H

#include <chrono>

#include <torrent/common.h>

namespace torrent {

class group_entry;

class choke_status {
public:
  group_entry*        entry() const                           { return m_group_entry; }
  void                set_entry(group_entry* grp_ent)         { m_group_entry = grp_ent; }

  bool                queued() const                          { return m_queued; }
  void                set_queued(bool s)                      { m_queued = s; }

  bool                choked() const                          { return !m_unchoked; }
  bool                unchoked() const                        { return m_unchoked; }
  void                set_unchoked(bool s)                    { m_unchoked = s; }

  bool                snubbed() const                         { return m_snubbed; }
  void                set_snubbed(bool s)                     { m_snubbed = s; }

  auto                time_last_choke() const                          { return m_time_last_choke; }
  void                set_time_last_choke(std::chrono::microseconds t) { m_time_last_choke = t; }

private:
  // TODO: Use flags.
  group_entry*        m_group_entry{};

  bool                m_queued{false};
  bool                m_unchoked{false};
  bool                m_snubbed{false};

  std::chrono::microseconds m_time_last_choke{};
};

} // namespace torrent

#endif
