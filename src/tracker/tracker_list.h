#ifndef LIBTORRENT_TRACKER_LIST_H
#define LIBTORRENT_TRACKER_LIST_H

// TODO: Reduce includes, don't inline everything.

#include <algorithm>
#include <functional>
#include <vector>
#include <torrent/tracker/tracker.h>

struct TestTrackerListWrapper;

namespace torrent {

// The tracker list will contain a list of tracker, divided into
// subgroups. Each group must be randomized before we start. When
// starting the tracker request, always start from the beginning and
// iterate if the request failed. Upon request success move the
// tracker to the beginning of the subgroup and start from the
// beginning of the whole list.

// TODO: Use tracker::Tracker non-pointer, change slots, etc into using TrackerWorker*.

class LIBTORRENT_EXPORT TrackerList : private std::vector<tracker::Tracker> {
public:
  using base_type = std::vector<tracker::Tracker>;

  using slot_tracker      = std::function<void(tracker::Tracker)>;
  using slot_string       = std::function<void(tracker::Tracker, const std::string&)>;
  using slot_address_list = std::function<uint32_t(tracker::Tracker, AddressList*)>;

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
  using base_type::operator[];

  TrackerList();
  ~TrackerList();

  TrackerList(const TrackerList&) = delete;
  TrackerList& operator=(const TrackerList&) = delete;

  bool                has_active() const;
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
  // TODO: Replace with shared_ptr to TrackerWorker.
  void                receive_success(tracker::Tracker tracker, AddressList* l);
  void                receive_failed(tracker::Tracker tracker, const std::string& msg);

  void                receive_scrape_success(tracker::Tracker tracker);
  void                receive_scrape_failed(tracker::Tracker tracker, const std::string& msg);

  // Used by libtorrent internally.
  slot_address_list&  slot_success()                          { return m_slot_success; }
  slot_string&        slot_failure()                          { return m_slot_failed; }

  slot_tracker&       slot_scrape_success()                   { return m_slot_scrape_success; }
  slot_string&        slot_scrape_failure()                   { return m_slot_scrape_failed; }

  slot_tracker&       slot_tracker_enabled()                  { return m_slot_tracker_enabled; }
  slot_tracker&       slot_tracker_disabled()                 { return m_slot_tracker_disabled; }

protected:
  friend class DownloadWrapper;
  friend struct ::TestTrackerListWrapper;

  void                set_info(DownloadInfo* info)            { m_info = info; }
  void                set_state(int s)                        { m_state = s; }
  void                set_key(uint32_t k)                     { m_key = k; }

private:
  DownloadInfo*       m_info{nullptr};
  int                 m_state;

  // TODO: Key should be part of download static info.
  uint32_t            m_key{0};
  int32_t             m_numwant{-1};

  slot_address_list   m_slot_success;
  slot_string         m_slot_failed;

  slot_tracker        m_slot_scrape_success;
  slot_string         m_slot_scrape_failed;

  slot_tracker        m_slot_tracker_enabled;
  slot_tracker        m_slot_tracker_disabled;
};

} // namespace torrent

#endif
