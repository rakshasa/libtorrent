#ifndef LIBTORRENT_DOWNLOAD_INFO_H
#define LIBTORRENT_DOWNLOAD_INFO_H

#include <cinttypes>
#include <functional>
#include <list>
#include <string>

#include <torrent/rate.h>
#include <torrent/hash_string.h>

#include <rak/timer.h>

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

  static const int flag_open                = (1 << 0);
  static const int flag_active              = (1 << 1);
  static const int flag_accepting_new_peers = (1 << 3);
  static const int flag_accepting_seeders   = (1 << 4); // Only used during leeching.
  static const int flag_private             = (1 << 5);
  static const int flag_meta_download       = (1 << 6);
  static const int flag_pex_enabled         = (1 << 7);
  static const int flag_pex_active          = (1 << 8);

  static const int public_flags = flag_accepting_seeders;

  static const uint32_t unlimited = ~uint32_t();

  const std::string&  name() const                                 { return m_name; }
  void                set_name(const std::string& s)               { m_name = s; }

  const HashString&   hash() const                                 { return m_hash; }
  const HashString&   info_hash() const                            { return m_hash; }
  HashString&         mutable_hash()                               { return m_hash; }

  const HashString&   hash_obfuscated() const                      { return m_hashObfuscated; }
  const HashString&   info_hash_obfuscated() const                 { return m_hashObfuscated; }
  HashString&         mutable_hash_obfuscated()                    { return m_hashObfuscated; }

  const HashString&   local_id() const                             { return m_localId; }
  HashString&         mutable_local_id()                           { return m_localId; }

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

  const Rate*         up_rate() const                              { return &m_upRate; }
  const Rate*         down_rate() const                            { return &m_downRate; }
  const Rate*         skip_rate() const                            { return &m_skipRate; }

  Rate*               mutable_up_rate() const                      { return &m_upRate; }
  Rate*               mutable_down_rate() const                    { return &m_downRate; }
  Rate*               mutable_skip_rate() const                    { return &m_skipRate; }

  uint64_t            uploaded_baseline() const                    { return m_uploadedBaseline; }
  uint64_t            uploaded_adjusted() const                    { return std::max<int64_t>(m_upRate.total() - uploaded_baseline(), 0); }
  void                set_uploaded_baseline(uint64_t b)            { m_uploadedBaseline = b; }

  uint64_t            completed_baseline() const                   { return m_completedBaseline; }
  uint64_t            completed_adjusted() const                   { return std::max<int64_t>(m_slotStatCompleted() - completed_baseline(), 0); }
  void                set_completed_baseline(uint64_t b)           { m_completedBaseline = b; }

  size_t              metadata_size() const                        { return m_metadataSize; }
  void                set_metadata_size(size_t size)               { m_metadataSize = size; }

  uint32_t            size_pex() const                             { return m_sizePex; }
  void                set_size_pex(uint32_t b)                     { m_sizePex = b; }

  uint32_t            max_size_pex() const                         { return m_maxSizePex; }
  void                set_max_size_pex(uint32_t b)                 { m_maxSizePex = b; }

  uint32_t            max_size_pex_list() const                    { return 200; }

  // Unix epoche, 0 == unknown.
  uint32_t            creation_date() const                        { return m_creationDate; }
  uint32_t            load_date() const                            { return m_loadDate; }

  uint32_t            upload_unchoked() const                      { return m_upload_unchoked; }
  uint32_t            download_unchoked() const                    { return m_download_unchoked; }

  // The list of addresses is guaranteed to be sorted and unique.
  signal_void_type&   signal_tracker_success() const               { return m_signalTrackerSuccess; }
  signal_string_type& signal_tracker_failed() const                { return m_signalTrackerFailed; }

  //
  // Libtorrent internal:
  //

  void                set_creation_date(uint32_t d)                { m_creationDate = d; }

  void                set_upload_unchoked(uint32_t num)            { m_upload_unchoked = num; }
  void                set_download_unchoked(uint32_t num)          { m_download_unchoked = num; }

  slot_stat_type&     slot_left()                                  { return m_slotStatLeft; }
  slot_stat_type&     slot_completed()                             { return m_slotStatCompleted; }

private:
  std::string         m_name;
  HashString          m_hash{HashString::new_zero()};
  HashString          m_hashObfuscated{HashString::new_zero()};
  HashString          m_localId{HashString::new_zero()};

  mutable int         m_flags{flag_accepting_new_peers | flag_accepting_seeders | flag_pex_enabled | flag_pex_active};

  mutable Rate        m_upRate{60};
  mutable Rate        m_downRate{60};
  mutable Rate        m_skipRate{60};

  uint64_t            m_uploadedBaseline{0};
  uint64_t            m_completedBaseline{0};
  uint32_t            m_sizePex{0};
  uint32_t            m_maxSizePex{8};
  size_t              m_metadataSize{0};

  uint32_t            m_creationDate{0};
  uint32_t            m_loadDate = rak::timer::current_seconds();

  uint32_t            m_upload_unchoked{0};
  uint32_t            m_download_unchoked{0};

  slot_stat_type      m_slotStatLeft;
  slot_stat_type      m_slotStatCompleted;

  mutable signal_void_type    m_signalTrackerSuccess;
  mutable signal_string_type  m_signalTrackerFailed;
};

}

#endif
