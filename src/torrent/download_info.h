#ifndef LIBTORRENT_DOWNLOAD_INFO_H
#define LIBTORRENT_DOWNLOAD_INFO_H

#include <cinttypes>
#include <functional>
#include <list>
#include <string>

#include <torrent/rate.h>
#include <torrent/hash_string.h>

namespace torrent {

class FileList;
class DownloadMain;

// This will become a Download 'handle' of kinds.

// TODO: Split into DownloadInfo and DownloadState.
// TODO: Rename 'hash' to 'info_hash'.

class LIBTORRENT_EXPORT DownloadInfo {
public:
  using slot_stat_type = std::function<uint64_t()>;

  using signal_void_type   = std::list<std::function<void()>>;
  using signal_string_type = std::list<std::function<void(const std::string&)>>;

  enum State {
    NONE,
    COMPLETED,
    STARTED,
    STOPPED
  };

  static constexpr int flag_open                = (1 << 0);
  static constexpr int flag_active              = (1 << 1);
  static constexpr int flag_accepting_new_peers = (1 << 3);
  static constexpr int flag_accepting_seeders   = (1 << 4); // Only used during leeching.
  static constexpr int flag_private             = (1 << 5);
  static constexpr int flag_meta_download       = (1 << 6);
  static constexpr int flag_pex_enabled         = (1 << 7);
  static constexpr int flag_pex_active          = (1 << 8);

  static constexpr int public_flags = flag_accepting_seeders;

  static constexpr uint32_t unlimited = ~uint32_t();

  const std::string&  name() const                                 { return m_name; }
  void                set_name(const std::string& s)               { m_name = s; }

  const HashString&   hash() const                                 { return m_hash; }
  const HashString&   info_hash() const                            { return m_hash; }
  HashString&         mutable_hash()                               { return m_hash; }

  const HashString&   hash_obfuscated() const                      { return m_hash_obfuscated; }
  const HashString&   info_hash_obfuscated() const                 { return m_hash_obfuscated; }
  HashString&         mutable_hash_obfuscated()                    { return m_hash_obfuscated; }

  const HashString&   local_id() const                             { return m_local_id; }
  HashString&         mutable_local_id()                           { return m_local_id; }

  bool                is_open() const                              { return m_flags & flag_open; }
  bool                is_active() const                            { return m_flags & flag_active; }
  bool                is_accepting_new_peers() const               { return m_flags & flag_accepting_new_peers; }
  bool                is_accepting_seeders() const                 { return m_flags & flag_accepting_seeders; }
  bool                is_private() const                           { return m_flags & flag_private; }
  bool                is_meta_download() const                     { return m_flags & flag_meta_download; }
  bool                is_pex_enabled() const                       { return m_flags & flag_pex_enabled; }
  bool                is_pex_active() const                        { return m_flags & flag_pex_active; }

  int                 flags() const                                { return m_flags; }

  void                set_flags(int flags)                         { m_flags |= flags; }
  void                unset_flags(int flags)                       { m_flags &= ~flags; }
  void                change_flags(int flags, bool state)          { if (state) set_flags(flags); else unset_flags(flags); }

  void                public_set_flags(int flags) const                { m_flags |= (flags & public_flags); }
  void                public_unset_flags(int flags) const              { m_flags &= ~(flags & public_flags); }
  void                public_change_flags(int flags, bool state) const { if (state) public_set_flags(flags); else public_unset_flags(flags); }

  void                set_private()                                { set_flags(flag_private); unset_flags(flag_pex_enabled); }
  void                set_pex_enabled()                            { if (!is_private()) set_flags(flag_pex_enabled); }

  const Rate*         up_rate() const                              { return &m_up_rate; }
  const Rate*         down_rate() const                            { return &m_down_rate; }
  const Rate*         skip_rate() const                            { return &m_skip_rate; }

  Rate*               mutable_up_rate() const                      { return &m_up_rate; }
  Rate*               mutable_down_rate() const                    { return &m_down_rate; }
  Rate*               mutable_skip_rate() const                    { return &m_skip_rate; }

  uint64_t            uploaded_baseline() const                    { return m_uploaded_baseline; }
  uint64_t            uploaded_adjusted() const                    { return std::max<int64_t>(m_up_rate.total() - uploaded_baseline(), 0); }
  void                set_uploaded_baseline(uint64_t b)            { m_uploaded_baseline = b; }

  uint64_t            completed_baseline() const                   { return m_completed_baseline; }
  uint64_t            completed_adjusted() const                   { return std::max<int64_t>(m_slot_stat_completed() - completed_baseline(), 0); }
  void                set_completed_baseline(uint64_t b)           { m_completed_baseline = b; }

  size_t              metadata_size() const                        { return m_metadata_size; }
  void                set_metadata_size(size_t size)               { m_metadata_size = size; }

  uint32_t            size_pex() const                             { return m_size_pex; }
  void                set_size_pex(uint32_t b)                     { m_size_pex = b; }

  uint32_t            max_size_pex() const                         { return m_max_size_pex; }
  void                set_max_size_pex(uint32_t b)                 { m_max_size_pex = b; }

  static uint32_t     max_size_pex_list()                          { return 200; }

  // Unix epoche, 0 == unknown.
  uint32_t            creation_date() const                        { return m_creation_date; }
  uint32_t            load_date() const                            { return m_load_date; }

  uint32_t            upload_unchoked() const                      { return m_upload_unchoked; }
  uint32_t            download_unchoked() const                    { return m_download_unchoked; }

  // The list of addresses is guaranteed to be sorted and unique.
  signal_void_type&   signal_tracker_success() const               { return m_signal_tracker_success; }
  signal_string_type& signal_tracker_failed() const                { return m_signal_tracker_failed; }

  //
  // Libtorrent internal:
  //

  void                set_creation_date(uint32_t d)                { m_creation_date = d; }

  void                set_upload_unchoked(uint32_t num)            { m_upload_unchoked = num; }
  void                set_download_unchoked(uint32_t num)          { m_download_unchoked = num; }

  slot_stat_type&     slot_left()                                  { return m_slot_stat_left; }
  slot_stat_type&     slot_completed()                             { return m_slot_stat_completed; }

protected:
  friend class DownloadMain;

  void                set_load_date(uint32_t d)                    { m_load_date = d; }

private:
  std::string         m_name;
  HashString          m_hash{HashString::new_zero()};
  HashString          m_hash_obfuscated{HashString::new_zero()};
  HashString          m_local_id{HashString::new_zero()};

  mutable int         m_flags{flag_accepting_new_peers | flag_accepting_seeders | flag_pex_enabled | flag_pex_active};

  mutable Rate        m_up_rate{60};
  mutable Rate        m_down_rate{60};
  mutable Rate        m_skip_rate{60};

  uint64_t            m_uploaded_baseline{};
  uint64_t            m_completed_baseline{};
  uint32_t            m_size_pex{};
  uint32_t            m_max_size_pex{8};
  size_t              m_metadata_size{};

  uint32_t            m_creation_date{};
  uint32_t            m_load_date{};

  uint32_t            m_upload_unchoked{};
  uint32_t            m_download_unchoked{};

  slot_stat_type      m_slot_stat_left;
  slot_stat_type      m_slot_stat_completed;

  mutable signal_void_type   m_signal_tracker_success;
  mutable signal_string_type m_signal_tracker_failed;
};

} // namespace torrent

#endif
