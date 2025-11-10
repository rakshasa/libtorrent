#ifndef LIBTORRENT_DHT_SERVER_H
#define LIBTORRENT_DHT_SERVER_H

#include <array>
#include <deque>
#include <map>

#include "dht/dht_transaction.h"
#include "net/socket_datagram.h"
#include "torrent/hash_string.h"
#include "torrent/object_raw_bencode.h"
#include "torrent/utils/scheduler.h"

namespace torrent {

class DhtBucket;
class DhtNode;
class DhtRouter;

class DownloadInfo;
class DhtMessage;
class TrackerDht;

// UDP server that handles the DHT node communications.

class DhtServer : public SocketDatagram {
public:
  DhtServer(DhtRouter* self);
  ~DhtServer() override;

  const char*         type_name() const override         { return "dht"; }

  bool                is_active() const                  { return get_fd().is_valid(); }

  void                start(int port);
  void                stop();

  unsigned int        queries_received() const           { return m_queriesReceived; }
  unsigned int        queries_sent() const               { return m_queriesSent; }
  unsigned int        replies_received() const           { return m_repliesReceived; }
  unsigned int        errors_received() const            { return m_errorsReceived; }
  unsigned int        errors_caught() const              { return m_errorsCaught; }
  void                reset_statistics();

  // Contact a node to see if it replies. Set id=0 if unknown.
  void                ping(const HashString& id, const sockaddr* sa);

  // Do a find_node search with the given contacts as starting point for the
  // search.
  void                find_node(const DhtBucket& contacts, const HashString& target);

  // Do DHT announce, starting with the given contacts.
  void                announce(const DhtBucket& contacts, const HashString& infoHash, TrackerDht* tracker);

  // Cancel given announce for given tracker, or all matching announces if info/tracker NULL.
  void                cancel_announce(const HashString* info_hash, const TrackerDht* tracker);

  // Called every 15 minutes.
  void                update();

  void                event_read() override;
  void                event_write() override;
  void                event_error() override;

private:
  // DHT error codes.
  static constexpr int dht_error_generic    = 201;
  static constexpr int dht_error_server     = 202;
  static constexpr int dht_error_protocol   = 203;
  static constexpr int dht_error_bad_method = 204;

  struct [[gnu::packed]] compact_node_info {
    char                 _id[20];
    SocketAddressCompact _addr;

    HashString&          id()            { return *HashString::cast_from(_id); }
  };

  using packet_queue   = std::deque<std::shared_ptr<DhtTransactionPacket>>;
  using node_info_list = std::list<compact_node_info>;

  // Pending transactions.
  using transaction_map = std::map<DhtTransaction::key_type, std::shared_ptr<DhtTransaction>>;
  using transaction_itr = transaction_map::iterator;

  // DHT transaction names for given transaction type.
  static constexpr std::array queries{
    "ping",
    "find_node",
    "get_peers",
    "announce_peer",
  };

  // Priorities for the outgoing packets.
  static constexpr int packet_prio_high  = 2;  // For important queries we send (announces).
  static constexpr int packet_prio_low   = 1;  // For (relatively) unimportant queries we send.
  static constexpr int packet_prio_reply = 0;  // For replies to peer queries.

  void                start_write();

  void                process_query(const HashString& id, const sockaddr* sa, const DhtMessage& req);
  void                process_response(const HashString& id, const sockaddr* sa, const DhtMessage& req);
  void                process_error(const sockaddr* sa, const DhtMessage& error);

  void                parse_find_node_reply(DhtTransactionSearch* t, raw_string nodes);
  void                parse_get_peers_reply(DhtTransactionGetPeers* t, const DhtMessage& res);

  void                find_node_next(DhtTransactionSearch* t);

  void                add_packet(std::shared_ptr<DhtTransactionPacket> packet, int priority);
  void                drop_packet(DhtTransactionPacket* packet);

  void                create_query(transaction_itr itr, int tID, const sockaddr* sa, int priority);
  void                create_response(const DhtMessage& req, const sockaddr* sa, DhtMessage& reply);
  void                create_error(const DhtMessage& req, const sockaddr* sa, int num, const char* msg);

  void                create_find_node_response(const DhtMessage& arg, DhtMessage& reply);
  void                create_get_peers_response(const DhtMessage& arg, const sockaddr* sa, DhtMessage& reply);
  void                create_announce_peer_response(const DhtMessage& arg, const sockaddr* sa, DhtMessage& reply);

  int                 add_transaction(std::shared_ptr<DhtTransaction> transaction, int priority);

  // This returns the iterator after the given one or end()
  transaction_itr     failed_transaction(transaction_itr itr, bool quick);

  void                clear_transactions();

  void                process_queue(packet_queue& queue);
  void                receive_timeout();

  DhtRouter*          m_router{};
  packet_queue        m_highQueue;
  packet_queue        m_lowQueue;
  transaction_map     m_transactions;

  utils::SchedulerEntry m_task_timeout;

  unsigned int        m_queriesReceived{};
  unsigned int        m_queriesSent{};
  unsigned int        m_repliesReceived{};
  unsigned int        m_errorsReceived{};
  unsigned int        m_errorsCaught{};

  bool                m_networkUp{false};
};

} // namespace torrent

#endif
