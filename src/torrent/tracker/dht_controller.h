#ifndef LIBTORRENT_TRACKER_DHT_CONTROLLER_H
#define LIBTORRENT_TRACKER_DHT_CONTROLLER_H

#include <atomic>
#include <memory>
#include <mutex>
#include <torrent/common.h>

namespace torrent {

class TrackerDht;

} // namespace torrent

namespace torrent::tracker {

class LIBTORRENT_EXPORT DhtController {
public:
  struct statistics_type {
    // Cycle; 0=inactive, 1=initial bootstrapping, 2 and up=normal operation
    unsigned int       cycle{};

    // DHT query statistics.
    unsigned int       queries_received{};
    unsigned int       queries_sent{};
    unsigned int       replies_received{};
    unsigned int       errors_received{};
    unsigned int       errors_caught{};

    // DHT node info.
    unsigned int       num_nodes{};
    unsigned int       num_buckets{};

    // DHT tracker info.
    unsigned int       num_peers{};
    unsigned int       max_peers{};
    unsigned int       num_trackers{};

    // The IPv6 DHT (BEP 32); the fields above are the IPv4 DHT's, or the IPv6 one's when IPv4 is not running.
    bool               active6{};
    unsigned int       cycle6{};
    unsigned int       queries_received6{};
    unsigned int       queries_sent6{};
    unsigned int       replies_received6{};
    unsigned int       num_nodes6{};
    unsigned int       num_buckets6{};
    unsigned int       num_peers6{};
    unsigned int       num_trackers6{};
  };

  DhtController();
  ~DhtController();

  // Thread-safe:

  bool                is_valid();
  bool                is_active();
  bool                is_receiving_requests();
  bool                is_nodes_populated();

  void                set_receive_requests(bool state);

  // TODO: This is the active port, move this to NetworkConfig? (or make this unavailable)
  uint16_t            port();

  // Main thread:

  void                initialize(const Object& dht_cache);

  bool                start();
  void                stop();

  // Store DHT cache in the given container and return the container.
  Object*             store_cache(Object* container);

  // Add a node by host (from a torrent file), or by address from explicit add_node
  // command or the BT PORT message. The address picks the family's router; add_peer_node skips
  // a family whose table is already populated.
  void                add_bootstrap_node(std::string host, int port);
  void                add_node(const sockaddr* sa, int port);
  void                add_peer_node(const sockaddr* sa, int port);

  statistics_type     get_statistics();
  void                reset_statistics();

protected:
  friend class torrent::DhtRouter;
  friend class torrent::TrackerDht;

  void                set_nodes_populated(int family, bool state);

  // Called from tracker_thread.
  void                announce(const HashString& info_hash, std::weak_ptr<TrackerDht> weak_tracker);
  void                cancel_announce(const HashString& info_hash, std::weak_ptr<TrackerDht> weak_tracker);

private:
  DhtRouter*          router_for_unsafe(const sockaddr* sa);

  std::atomic<bool>   m_nodes_populated{false};
  std::atomic<bool>   m_nodes_populated6{false};

  std::mutex          m_lock;
  uint16_t            m_port{0};
  bool                m_receive_requests{true};

  std::unique_ptr<DhtRouter> m_router;
  std::unique_ptr<DhtRouter> m_router6;
};

inline bool
DhtController::is_nodes_populated() {
  return m_nodes_populated.load(std::memory_order_relaxed) || m_nodes_populated6.load(std::memory_order_relaxed);
}

} // namespace torrent::tracker

#endif // LIBTORRENT_TRACKER_DHT_CONTROLLER_H
