// Temporary classes for thread-safe tracker related classes.
//
// These will be renamed after movimg the old torrent/tracker_* classes to src/tracker.

#ifndef LIBTORRENT_TRACKER_WRAPPER_H
#define LIBTORRENT_TRACKER_WRAPPER_H

#include <functional>
#include <memory>
#include <torrent/common.h>
#include <torrent/hash_string.h>

// TODO: Rename to TrackerController when torrent::TrackerController is moved.

namespace torrent::tracker {

class LIBTORRENT_EXPORT TrackerControllerWrapper {
public:
  using ptr_type          = std::shared_ptr<TrackerController>;
  using slot_string       = std::function<void(const std::string&)>;
  using slot_address_list = std::function<uint32_t(AddressList*)>;

  TrackerControllerWrapper() = default;
  TrackerControllerWrapper(const HashString& info_hash, std::shared_ptr<TrackerController>&& controller);

  bool                is_valid() const  { return m_ptr != nullptr; }

  bool                is_active() const;
  bool                is_requesting() const;
  bool                is_failure_mode() const;
  bool                is_promiscuous_mode() const;

  bool                has_active_trackers() const;
  bool                has_active_trackers_not_scrape() const;
  bool                has_usable_trackers() const;

  const HashString&   info_hash() const { return m_info_hash; }

  uint32_t            key() const;

  int32_t             numwant() const;
  void                set_numwant(int32_t n);

  uint32_t            size() const;
  Tracker             at(uint32_t index) const;

  void                add_extra_tracker(uint32_t group, const std::string& url);

  uint32_t            seconds_to_next_timeout() const;
  uint32_t            seconds_to_next_scrape() const;

  void                manual_request(bool request_now);
  void                scrape_request(uint32_t seconds_to_request);

  void                cycle_group(uint32_t group);

  // Algorithms:
  Tracker             find_if(const std::function<bool(Tracker&)>& f);
  void                for_each(const std::function<void(Tracker&)>& f);
  Tracker             c_find_if(const std::function<bool(const Tracker&)>& f) const;
  void                c_for_each(const std::function<void(const Tracker&)>& f) const;

  bool operator<(const TrackerControllerWrapper& rhs) const;

protected:
  friend class torrent::Download;
  friend class torrent::DownloadMain;
  friend class torrent::DownloadWrapper;
  friend class Manager;

  TrackerController*       get()        { return m_ptr.get(); }
  const TrackerController* get() const  { return m_ptr.get(); }
  ptr_type&                get_shared() { return m_ptr; }

  void                enable();
  void                enable_dont_reset_stats();
  void                disable();

  void                close();

  void                send_start_event();
  void                send_stop_event();
  void                send_completed_event();
  void                send_update_event();

  void                start_requesting();
  void                stop_requesting();

  void                set_slots(slot_address_list success, slot_string failure);

private:
  ptr_type            m_ptr;
  HashString          m_info_hash;
};

inline
TrackerControllerWrapper::TrackerControllerWrapper(const HashString& info_hash, std::shared_ptr<TrackerController>&& controller) :
  m_ptr(std::move(controller)),
  m_info_hash(info_hash) {
}

inline bool
TrackerControllerWrapper::operator<(const TrackerControllerWrapper& rhs) const {
  return this->get() < rhs.get();
}

} // namespace torrent::tracker

#endif // LIBTORRENT_TRACKER_TRACKER_WRAPPER_H
