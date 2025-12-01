#ifndef LIBTORRENT_TRACKER_LIST_H
#define LIBTORRENT_TRACKER_LIST_H

// TODO: Reduce includes, don't inline everything.

#include <algorithm>
#include <functional>
#include <vector>
#include <torrent/tracker/tracker.h>

struct TestTrackerListWrapper;

namespace torrent {

// The tracker list will contain a list of tracker, divided into subgroups.
//
// Each group must be randomized before we start.
//
// When starting the tracker request, always start from the beginning and iterate if the request
// failed.
//
// Upon request success move the tracker to the beginning of the subgroup and start from the
// beginning of the whole list.

class LIBTORRENT_EXPORT TrackerList : private std::vector<tracker::Tracker> {
public:
  using base_type = std::vector<tracker::Tracker>;

  using base_type::value_type;

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
  using base_type::front;
  using base_type::back;

  using base_type::at;

  TrackerList();
  ~TrackerList();

  bool                has_active() const;
  bool                has_active_not_dht() const;
  bool                has_active_not_scrape() const;
  bool                has_active_in_group(uint32_t group) const;
  bool                has_active_not_scrape_in_group(uint32_t group) const;
  bool                has_usable() const;

  void                close_all() { close_all_excluding(0); }
  void                close_all_excluding(int event_bitmap);

  void                clear();
  void                clear_stats();

  iterator            insert(unsigned int group, const tracker::Tracker& tracker);
  void                insert_url(unsigned int group, const std::string& url, bool extra_tracker = false);

  // TODO: Move these to controller / tracker.

  // TODO: CHECK PEX CAUSES PEER CONNECTS.

  void                send_event(tracker::Tracker& tracker, tracker::TrackerState::event_enum new_event);

  void                send_scrape(tracker::Tracker& tracker);

  const DownloadInfo* info() const                            { return m_info; }
  int                 state() const                           { return m_state; }
  uint32_t            key() const                             { return m_key; }

  int32_t             numwant() const                         { return m_numwant; }
  void                set_numwant(int32_t n)                  { m_numwant = n; }

  iterator            find(const tracker::Tracker& tb)        { return std::find(begin(), end(), tb); }
  iterator            find_url(const std::string& url);

  iterator            find_next_to_request(iterator itr);

  iterator            begin_group(unsigned int group);
  const_iterator      begin_group(unsigned int group) const;
  iterator            end_group(unsigned int group)           { return begin_group(group + 1); }
  const_iterator      end_group(unsigned int group) const     { return begin_group(group + 1); }

  size_type           size_group() const;
  void                cycle_group(unsigned int group);

  iterator            promote(iterator itr);
  void                randomize_group_entries();

  // TODO: Make protected.
  void                receive_success(tracker::Tracker tracker, AddressList* l);
  void                receive_failed(tracker::Tracker tracker, const std::string& msg);
  void                receive_scrape_success(tracker::Tracker tracker);
  void                receive_scrape_failed(tracker::Tracker tracker, const std::string& msg);
  void                receive_new_peers(tracker::Tracker tracker, AddressList* l);

  auto&               slot_success()                          { return m_slot_success; }
  auto&               slot_failure()                          { return m_slot_failed; }
  auto&               slot_scrape_success()                   { return m_slot_scrape_success; }
  auto&               slot_scrape_failure()                   { return m_slot_scrape_failed; }
  auto&               slot_new_peers()                        { return m_slot_new_peers; }

  auto&               slot_tracker_enabled()                  { return m_slot_tracker_enabled; }
  auto&               slot_tracker_disabled()                 { return m_slot_tracker_disabled; }

protected:
  friend class DownloadWrapper;
  friend struct ::TestTrackerListWrapper;

  void                set_info(DownloadInfo* info)            { m_info = info; }
  void                set_state(int s)                        { m_state = s; }
  void                set_key(uint32_t k)                     { m_key = k; }

private:
  TrackerList(const TrackerList&) = delete;
  TrackerList& operator=(const TrackerList&) = delete;

  DownloadInfo*       m_info{};
  int                 m_state{};

  // TODO: Key should be part of download static info.
  uint32_t            m_key{0};
  int32_t             m_numwant{-1};

  std::function<uint32_t(tracker::Tracker, AddressList*)>   m_slot_success;
  std::function<void(tracker::Tracker, const std::string&)> m_slot_failed;
  std::function<void(tracker::Tracker)>                     m_slot_scrape_success;
  std::function<void(tracker::Tracker, const std::string&)> m_slot_scrape_failed;
  std::function<uint32_t(AddressList*)>                     m_slot_new_peers;
  std::function<void(tracker::Tracker)>                     m_slot_tracker_enabled;
  std::function<void(tracker::Tracker)>                     m_slot_tracker_disabled;
};

} // namespace torrent

#endif
