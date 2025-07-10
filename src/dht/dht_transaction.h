// libTorrent - BitTorrent library
// Copyright (C) 2005-2011, Jari Sundell
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
// In addition, as a special exception, the copyright holders give
// permission to link the code of portions of this program with the
// OpenSSL library under certain conditions as described in each
// individual source file, and distribute linked combinations
// including the two.
//
// You must obey the GNU General Public License in all respects for
// all of the code used other than OpenSSL.  If you modify file(s)
// with this exception, you may extend this exception to your version
// of the file(s), but you are not obligated to do so.  If you do not
// wish to do so, delete this exception statement from your version.
// If you delete this exception statement from all source files in the
// program, then also delete it here.
//
// Contact:  Jari Sundell <jaris@ifi.uio.no>
//
//           Skomakerveien 33
//           3185 Skoppum, NORWAY

#ifndef LIBTORRENT_DHT_TRANSACTION_H
#define LIBTORRENT_DHT_TRANSACTION_H

#include <map>
#include <memory>
#include <rak/socket_address.h>

#include "dht/dht_node.h"
#include "torrent/hash_string.h"
#include "torrent/object_static_map.h"
#include "tracker/tracker_dht.h"

namespace torrent {

class TrackerDht;

class DhtBucket;

class DhtSearch;
class DhtAnnounce;
class DhtTransactionSearch;

class DhtTransaction;
class DhtTransactionPing;
class DhtTransactionFindNode;
class DhtTransactionFindNodeAnnounce;
class DhtTransactionGetPeers;
class DhtTransactionAnnouncePeer;

// DhtSearch implements the DHT search algorithm and holds search data
// that needs to be persistent across multiple find_node transactions.
//
// DhtAnnounce is a derived class used for searches that will eventually
// lead to an announce to the closest nodes.


// Compare predicate for ID closeness.
struct dht_compare_closer {
  dht_compare_closer(const HashString& target) : m_target(target) { }

  bool operator () (const std::unique_ptr<DhtNode>& one, const std::unique_ptr<DhtNode>& two) const;

  const HashString&               target() const   { return m_target; }
  raw_string                      target_raw_string() const { return raw_string(m_target.data(), HashString::size_data); }

  private:
  const HashString&    m_target;
};

// DhtSearch contains a list of nodes sorted by closeness to the given target,
// and returns what nodes to contact with up to three concurrent transactions pending.
// The map element is the DhtSearch object itself to allow the returned accessors
// to know which search a given node belongs to.
class DhtSearch : protected std::map<std::unique_ptr<DhtNode>, DhtSearch*, dht_compare_closer> {
  friend class DhtTransactionSearch;

public:
  using base_type = std::map<std::unique_ptr<DhtNode>, DhtSearch*, dht_compare_closer>;

  // Number of closest potential contact nodes to keep.
  static constexpr unsigned int max_contacts = 18;

  // Number of closest nodes we actually announce to.
  static constexpr unsigned int max_announce = 3;

  DhtSearch(const HashString& target, const DhtBucket& contacts);
  virtual ~DhtSearch();

  // Wrapper for iterators, allowing more convenient access to the key
  // and element values, which also makes it easier to change the container
  // without having to modify much code using iterators.
  template <typename T>
  struct accessor_wrapper : public T {
    accessor_wrapper() = default;
    accessor_wrapper(const T& itr) : T(itr) { }

    const std::unique_ptr<DhtNode>& node() const     { return (**this).first; }
    DhtSearch*                      search() const   { return (**this).second; }
  };

  using const_accessor = accessor_wrapper<base_type::const_iterator>;
  using accessor       = accessor_wrapper<base_type::iterator>;

  // Add a potential node to contact for the search.
  bool                 add_contact(const HashString& id, const rak::socket_address* sa);
  void                 add_contacts(const DhtBucket& contacts);

  // Return next node to contact. Up to concurrent_searches nodes are returned,
  // and end() after that. Don't advance the accessor to get further contacts!
  const_accessor       get_contact();

  // Search statistics.
  int                  num_contacted() const             { return m_contacted; }
  int                  num_replied() const               { return m_replied; }

  bool                 start()                           { m_started = true; return m_pending; }
  bool                 complete() const                  { return m_started && !m_pending; }

  const HashString&    target() const                    { return m_target; }
  raw_string           target_raw_string() const         { return raw_string(m_target.data(), HashString::size_data); }

  virtual bool         is_announce() const               { return false; }

  // Expose the otherwise private end() function but return an accessor,
  // to allow code checking whether get_contact returned a valid accessor.
  const_accessor       end() const                       { return base_type::end(); }

  // Used by the sorting/comparison predicate to see which node is closer.
  static bool          is_closer(const HashString& one, const HashString& two, const HashString& target);

protected:
  void                 trim(bool final);
  void                 node_status(const std::unique_ptr<DhtNode>& n, bool success);
  static void          set_node_active(const std::unique_ptr<DhtNode>& n, bool active);

  // Statistics about contacted nodes.
  unsigned int         m_pending{0};
  unsigned int         m_contacted{0};
  unsigned int         m_replied{0};
  unsigned int         m_concurrency{3};

  bool                 m_restart{false};  // If true, trim nodes and reset m_next on the following get_contact call.
  bool                 m_started{false};

  // Next node to return in get_contact, is end() if we have no more contactable nodes.
  const_accessor       m_next;

private:
  DhtSearch(const DhtSearch&) = delete;
  DhtSearch& operator=(const DhtSearch&) = delete;

  static bool          node_uncontacted(const std::unique_ptr<DhtNode>& node);

  HashString           m_target;
};

class DhtAnnounce : public DhtSearch {
public:
  DhtAnnounce(const HashString& infoHash, TrackerDht* tracker, const DhtBucket& contacts)
    : DhtSearch(infoHash, contacts),
      m_tracker(tracker) { }
  ~DhtAnnounce() override;

  bool                 is_announce() const override      { return true; }

  const TrackerDht*    tracker() const                   { return m_tracker; }

  // Start announce and return final set of nodes in get_contact() calls.
  // This resets DhtSearch's completed() function, which now
  // counts announces instead.
  const_accessor       start_announce();

  void                 receive_peers(raw_list peers) { m_tracker->receive_peers(peers); }
  void                 update_status() { m_tracker->receive_progress(m_replied, m_contacted); }

private:
  TrackerDht*          m_tracker;
};

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
  DhtTransactionPacket(const rak::socket_address* s, const DhtMessage& d, unsigned int id, DhtTransaction* t)
    : m_sa(*s), m_id(id), m_transaction(t) { build_buffer(d); }

  // non-transaction packet
  DhtTransactionPacket(const rak::socket_address* s, const DhtMessage& d)
    : m_sa(*s), m_id(-this_thread::cached_seconds().count()) { build_buffer(d); }

  ~DhtTransactionPacket() = default;

  bool                        has_transaction() const   { return m_id >= -1; }
  bool                        has_failed() const        { return m_id == -1; }
  void                        set_failed()              { m_id = -1; }

  const rak::socket_address*  address() const           { return &m_sa; }
  rak::socket_address*        address()                 { return &m_sa; }

  const char*                 c_str() const             { return m_data.get(); }
  size_t                      length() const            { return m_length; }

  int                         id() const                { return m_id; }
  int                         age() const               { return has_transaction() ? 0 : this_thread::cached_seconds().count() + m_id; }
  const DhtTransaction*       transaction() const       { return m_transaction; }
  DhtTransaction*             transaction()             { return m_transaction; }

private:
  DhtTransactionPacket(const DhtTransactionPacket&) = delete;
  DhtTransactionPacket& operator=(const DhtTransactionPacket&) = delete;

  void                        build_buffer(const DhtMessage& data);

  rak::socket_address         m_sa;
  std::unique_ptr<char[]>     m_data;
  size_t                      m_length;
  int                         m_id;
  DhtTransaction*             m_transaction{};
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

  virtual transaction_type    type() const = 0;
  virtual bool                is_search()          { return false; }

  // Key to uniquely identify a transaction with given per-node transaction id.
  using key_type = uint64_t;

  key_type                    key(int id) const    { return key(&m_sa, id); }
  static key_type             key(const rak::socket_address* sa, int id);
  static bool                 key_match(key_type key, const rak::socket_address* sa);

  // Node ID and address.
  const HashString&           id()                 { return m_id; }
  const rak::socket_address*  address()            { return &m_sa; }

  int                         timeout() const           { return m_timeout; }
  int                         quick_timeout() const     { return m_quickTimeout; }
  bool                        has_quick_timeout() const { return m_hasQuickTimeout; }

  DhtTransactionPacket*       packet() const       { return m_packet; }
  void                        set_packet(DhtTransactionPacket* p) { m_packet = p; }

  DhtTransactionSearch*       as_search();

  DhtTransactionPing*         as_ping();
  DhtTransactionFindNode*     as_find_node();
  DhtTransactionGetPeers*     as_get_peers();
  DhtTransactionAnnouncePeer* as_announce_peer();

protected:
  DhtTransaction(int quick_timeout, int timeout, const HashString& id, const rak::socket_address* sa);

  // m_id must be the first element to ensure it is aligned properly,
  // because we later read a size_t value from it.
  const HashString       m_id;
  bool                   m_hasQuickTimeout;

private:
  DhtTransaction(const DhtTransaction&) = delete;
  DhtTransaction& operator=(const DhtTransaction&) = delete;

  rak::socket_address    m_sa;
  int                    m_timeout;
  int                    m_quickTimeout;
  DhtTransactionPacket*  m_packet{};
};

class DhtTransactionSearch : public DhtTransaction {
public:
  ~DhtTransactionSearch() override;

  bool                       is_search() override         { return true; }

  DhtSearch::const_accessor  node()                       { return m_node; }
  DhtSearch*                 search()                     { return m_search; }

  void                       set_stalled();

  void                       complete(bool success);

protected: 
  DhtTransactionSearch(int quick_timeout, int timeout, DhtSearch::const_accessor& node)
    : DhtTransaction(quick_timeout, timeout, node.node()->id(), node.node()->address()),
      m_node(node),
      m_search(node.search()) { if (!m_hasQuickTimeout) m_search->m_concurrency++; }

private:
  DhtSearch::const_accessor  m_node; 
  DhtSearch*                 m_search;
};

// Actual transaction classes.
class DhtTransactionPing : public DhtTransaction {
public:
  DhtTransactionPing(const HashString& id, const rak::socket_address* sa) 
    : DhtTransaction(-1, 30, id, sa) { }

  transaction_type            type() const override;
};

class DhtTransactionFindNode : public DhtTransactionSearch {
public:
  DhtTransactionFindNode(DhtSearch::const_accessor& node)
    : DhtTransactionSearch(4, 30, node) { }

  transaction_type           type() const override;
};

class DhtTransactionGetPeers : public DhtTransactionSearch {
public:
  DhtTransactionGetPeers(DhtSearch::const_accessor& node)
    : DhtTransactionSearch(-1, 30, node) { }

  transaction_type           type() const override;
};

class DhtTransactionAnnouncePeer : public DhtTransaction {
public:
  DhtTransactionAnnouncePeer(const HashString& id,
                             const rak::socket_address* sa,
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

inline bool
DhtSearch::is_closer(const HashString& one, const HashString& two, const HashString& target) {
  for (unsigned int i=0; i<torrent::HashString::size(); i++)
    if (one[i] != two[i])
      return static_cast<uint8_t>(one[i] ^ target[i]) < static_cast<uint8_t>(two[i] ^ target[i]);

  return false;
}

inline void
DhtSearch::set_node_active(const std::unique_ptr<DhtNode>& n, bool active) {
  n->m_lastSeen = active;
}

inline bool
dht_compare_closer::operator () (const std::unique_ptr<DhtNode>& one, const std::unique_ptr<DhtNode>& two) const {
  return DhtSearch::is_closer(*one, *two, m_target);
}

inline DhtTransaction::key_type
DhtTransaction::key(const rak::socket_address* sa, int id) {
  return (static_cast<uint64_t>(sa->sa_inet()->address_n()) << 32) + id;
}

inline bool
DhtTransaction::key_match(key_type key, const rak::socket_address* sa) {
  return (key >> 32) == sa->sa_inet()->address_n();
}

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
