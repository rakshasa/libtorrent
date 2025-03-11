#ifndef LIBTORRENT_TRACKER_DHT_CONTROLLER_H
#define LIBTORRENT_TRACKER_DHT_CONTROLLER_H

#include <memory>
#include <mutex>
#include <torrent/common.h>

namespace torrent {
class TrackerDht;
}

namespace torrent::tracker {

class LIBTORRENT_EXPORT DhtController {
public:
  struct statistics_type {
    // Cycle; 0=inactive, 1=initial bootstrapping, 2 and up=normal operation
    unsigned int       cycle;

    // UDP transfer rates.
    const Rate&        up_rate;
    const Rate&        down_rate;

    // DHT query statistics.
    unsigned int       queries_received;
    unsigned int       queries_sent;
    unsigned int       replies_received;
    unsigned int       errors_received;
    unsigned int       errors_caught;

    // DHT node info.
    unsigned int       num_nodes;
    unsigned int       num_buckets;

    // DHT tracker info.
    unsigned int       num_peers;
    unsigned int       max_peers;
    unsigned int       num_trackers;

    statistics_type(const Rate& up, const Rate& down) : up_rate(up), down_rate(down) { }
  };

  DhtController() = default;
  ~DhtController();

  // Thread-safe:

  bool                is_valid();
  bool                is_active();
  bool                is_receiving_requests();

  void                set_receive_requests(bool state);

  uint16_t            port();

  // Main thread:

  void                initialize(const Object& dhtCache);

  bool                start(uint16_t port);
  void                stop();

  // Store DHT cache in the given container and return the container.
  Object*             store_cache(Object* container);

  // Add a node by host (from a torrent file), or by address from explicit add_node
  // command or the BT PORT message.
  void                add_node(const std::string& host, int port);
  void                add_node(const sockaddr* addr, int port);

  statistics_type     get_statistics() const;
  void                reset_statistics();

  void                set_upload_throttle(Throttle* t);
  void                set_download_throttle(Throttle* t);

protected:
  friend class torrent::TrackerDht;

  // Thread-safe:

  void                announce(const HashString& info_hash, TrackerDht* tracker);
  void                cancel_announce(const HashString* info_hash, const torrent::TrackerDht* tracker);

private:
  std::mutex          m_lock;
  uint16_t            m_port{0};
  bool                m_receive_requests{true};

  std::unique_ptr<DhtRouter> m_router;
};

} // namespace torrent::tracker

#endif // LIBTORRENT_TRACKER_DHT_CONTROLLER_H
