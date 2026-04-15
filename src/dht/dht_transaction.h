#ifndef LIBTORRENT_DHT_TRANSACTION_H
#define LIBTORRENT_DHT_TRANSACTION_H

#include <map>
#include <memory>

#include "dht/dht_node.h"
#include "dht/transactions/dht_search.h"
#include "torrent/hash_string.h"
#include "torrent/object_static_map.h"
#include "torrent/net/types.h"
#include "tracker/tracker_dht.h"

namespace torrent::dht {

class DhtSearch;

}

namespace torrent {

class TrackerDht;

class DhtBucket;

class DhtTransactionSearch;

class DhtTransaction;
class DhtTransactionPing;
class DhtTransactionFindNode;
class DhtTransactionFindNodeAnnounce;
class DhtTransactionGetPeers;
class DhtTransactionAnnouncePeer;

// Possible bencode keys in a DHT message.
enum dht_keys {
  key_a_id,
  key_a_infoHash,
  key_a_port,
  key_a_target,
  key_a_token,

  key_e_0,
  key_e_1,

  key_q,

  key_r_id,
  key_r_nodes,
  key_r_token,
  key_r_values,

  key_t,
  key_v,
  key_y,

  key_LAST,
};

class DhtMessage : public static_map_type<dht_keys, key_LAST> {
public:
  using base_type = static_map_type<dht_keys, key_LAST>;

  // Must be big enough to hold one of the possible variable-sized reply data.
  // Currently either:
  // - error message (size doesn't really matter, it'll be truncated at worst)
  // - announce token (8 bytes, needs 20 bytes buffer to build)
  // Never more than one of the above.
  // And additionally for queries we send:
  // - transaction ID (3 bytes)
  static constexpr size_t data_size = 64;
  char data[data_size];
  char* data_end{data};
};

// Class holding transaction data to be transmitted.
class DhtTransactionPacket {
public:
  // transaction packet
  DhtTransactionPacket(const sockaddr* s, const DhtMessage& d, unsigned int id, std::shared_ptr<DhtTransaction> t);
  // non-transaction packet
  DhtTransactionPacket(const sockaddr* s, const DhtMessage& d);
  ~DhtTransactionPacket() = default;

  bool                has_transaction() const   { return m_id >= -1; }
  bool                has_failed() const        { return m_id == -1; }
  void                set_failed()              { m_id = -1; }

  const auto*         address() const           { return m_socket_address.get(); }
  auto*               address()                 { return m_socket_address.get(); }

  const char*         c_str() const             { return m_data.get(); }
  size_t              length() const            { return m_length; }

  int                 id() const                { return m_id; }
  int                 age() const               { return has_transaction() ? 0 : this_thread::cached_seconds().count() + m_id; }

  const auto*         transaction() const       { return m_transaction.get(); }
  auto*               transaction()             { return m_transaction.get(); }

private:
  DhtTransactionPacket(const DhtTransactionPacket&) = delete;
  DhtTransactionPacket& operator=(const DhtTransactionPacket&) = delete;

  void                build_buffer(const DhtMessage& data);

  sa_unique_ptr           m_socket_address;
  std::unique_ptr<char[]> m_data;
  size_t                  m_length{};
  int                     m_id{};

  std::shared_ptr<DhtTransaction> m_transaction;
};

// DHT Transaction classes. DhtTransaction and DhtTransactionSearch
// are not directly usable with no public constructor, since type()
// is a pure virtual function.
class DhtTransaction {
public:
  virtual ~DhtTransaction();

  enum transaction_type {
    DHT_PING,
    DHT_FIND_NODE,
    DHT_GET_PEERS,
    DHT_ANNOUNCE_PEER,
  };

  // Key to uniquely identify a transaction with given per-node transaction id.
  using key_type = uint64_t;

  virtual transaction_type type() const = 0;

  virtual bool        is_search()               { return false; }

  key_type            key(int id) const         { return key(m_socket_address.get(), id); }
  static key_type     key(const sockaddr* sa, int id);
  static bool         key_match(key_type key, const sockaddr* sa);

  const HashString&   id()                      { return m_id; }
  const auto*         address()                 { return m_socket_address.get(); }

  int                 timeout() const           { return m_timeout; }
  int                 quick_timeout() const     { return m_quickTimeout; }
  bool                has_quick_timeout() const { return m_hasQuickTimeout; }

  auto&               packet() const                                       { return m_packet; }
  void                set_packet(std::shared_ptr<DhtTransactionPacket>& p) { m_packet = p; }
  void                reset_packet()                                       { m_packet.reset(); }

  DhtTransactionSearch*       as_search();
  DhtTransactionPing*         as_ping();
  DhtTransactionFindNode*     as_find_node();
  DhtTransactionGetPeers*     as_get_peers();
  DhtTransactionAnnouncePeer* as_announce_peer();

protected:
  DhtTransaction(int quick_timeout, int timeout, const HashString& id, const sockaddr* sa);

  // m_id must be the first element to ensure it is aligned properly,
  // because we later read a size_t value from it.
  const HashString       m_id;
  bool                   m_hasQuickTimeout;

private:
  DhtTransaction(const DhtTransaction&) = delete;
  DhtTransaction& operator=(const DhtTransaction&) = delete;

  sa_unique_ptr          m_socket_address;
  int                    m_timeout;
  int                    m_quickTimeout;

  std::shared_ptr<DhtTransactionPacket> m_packet;
};

class DhtTransactionSearch : public DhtTransaction {
public:
  ~DhtTransactionSearch() override;

  bool                is_search() override         { return true; }

  auto                node()                       { return m_node; }
  auto&               search()                     { return m_search; }

  void                set_stalled();

  void                complete(bool success);

protected:
  DhtTransactionSearch(int quick_timeout, int timeout, dht::DhtSearch::const_accessor& node);

private:
  dht::DhtSearch::const_accessor  m_node;
  std::shared_ptr<dht::DhtSearch> m_search;
};

// Actual transaction classes.
class DhtTransactionPing : public DhtTransaction {
public:
  DhtTransactionPing(const HashString& id, const sockaddr* sa)
    : DhtTransaction(-1, 30, id, sa) { }

  transaction_type            type() const override;
};

class DhtTransactionFindNode : public DhtTransactionSearch {
public:
  DhtTransactionFindNode(dht::DhtSearch::const_accessor& node)
    : DhtTransactionSearch(4, 30, node) { }

  transaction_type           type() const override;
};

class DhtTransactionGetPeers : public DhtTransactionSearch {
public:
  DhtTransactionGetPeers(dht::DhtSearch::const_accessor& node)
    : DhtTransactionSearch(-1, 30, node) { }

  transaction_type           type() const override;
};

class DhtTransactionAnnouncePeer : public DhtTransaction {
public:
  DhtTransactionAnnouncePeer(const HashString& id,
                             const sockaddr* sa,
                             const HashString& infoHash,
                             raw_string token)
    : DhtTransaction(-1, 30, id, sa),
      m_infoHash(infoHash),
      m_token(token) { }

  transaction_type type() const override;

  const HashString&        info_hash() { return m_infoHash; }
  raw_string               info_hash_raw_string() const { return raw_string(m_infoHash.data(), HashString::size_data); }
  raw_string               token()     { return m_token; }

private:
  HashString m_infoHash;
  raw_string m_token;
};

// These could (should?) check that the type matches, or use dynamic_cast if we have RTTI.
inline DhtTransactionSearch*
DhtTransaction::as_search() {
  return static_cast<DhtTransactionSearch*>(this);
}

inline DhtTransactionPing*
DhtTransaction::as_ping() {
  return static_cast<DhtTransactionPing*>(this);
}

inline DhtTransactionFindNode*
DhtTransaction::as_find_node() {
  return static_cast<DhtTransactionFindNode*>(this);
}

inline DhtTransactionGetPeers*
DhtTransaction::as_get_peers() {
  return static_cast<DhtTransactionGetPeers*>(this);
}

inline DhtTransactionAnnouncePeer*
DhtTransaction::as_announce_peer() {
  return static_cast<DhtTransactionAnnouncePeer*>(this);
}

} // namespace torrent

#endif
