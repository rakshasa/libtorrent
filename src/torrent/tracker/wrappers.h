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
  typedef std::shared_ptr<TrackerController>       ptr_type;
  typedef std::function<void (const std::string&)> slot_string;
  typedef std::function<uint32_t (AddressList*)>   slot_address_list;

  TrackerControllerWrapper() = default;
  TrackerControllerWrapper(const HashString& info_hash, std::shared_ptr<TrackerController>&& controller);

  const HashString&   info_hash() const { return m_info_hash; }

  bool                is_active() const;
  bool                is_requesting() const;
  bool                is_failure_mode() const;
  bool                is_promiscuous_mode() const;

  uint32_t            seconds_to_next_timeout() const;
  uint32_t            seconds_to_next_scrape() const;

  void                manual_request(bool request_now);
  void                scrape_request(uint32_t seconds_to_request);

  bool operator<(const TrackerControllerWrapper& rhs) const;

protected:
  friend class torrent::Download;
  friend class torrent::DownloadMain;
  friend class torrent::DownloadWrapper;
  friend class TrackerManager;

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
  m_ptr(controller),
  m_info_hash(info_hash) {
}

inline bool
TrackerControllerWrapper::operator<(const TrackerControllerWrapper& rhs) const {
  return this->get() < rhs.get();
}

} // namespace torrent

#endif // LIBTORRENT_TRACKER_TRACKER_WRAPPER_H
