#ifndef LIBTORRENT_DHT_TRANSACTIONS_DHT_SEARCH_H
#define LIBTORRENT_DHT_TRANSACTIONS_DHT_SEARCH_H

#include <memory>

#include "torrent/common.h"
#include "torrent/hash_string.h"
#include "torrent/object_static_map.h"

// DhtSearch implements the DHT search algorithm and holds search data that needs to be persistent
// across multiple find_node transactions.
//
// DhtSearch contains a list of nodes sorted by closeness to the given target, and returns what
// nodes to contact with up to three concurrent transactions pending.
//
// The map element is the DhtSearch object itself to allow the returned accessors to know which
// search a given node belongs to.

// TODO: Consider moving this to dht/

namespace torrent {

class DhtBucket;
class DhtNode;
class DhtServer;
class DhtTransactionSearch;
class TrackerDht;

}

namespace torrent::dht {

// Compare predicate for ID closeness.
struct dht_compare_closer {
  dht_compare_closer(const HashString& target) : m_target(target) { }

  const HashString&   target() const            { return m_target; }
  raw_string          target_raw_string() const { return raw_string(m_target.data(), HashString::size_data); }

  bool operator () (const std::unique_ptr<DhtNode>& one, const std::unique_ptr<DhtNode>& two) const;

private:
  const HashString&   m_target;
};

// Use std::enable_shared_from_this as a temporary hack.

class DhtSearch : public std::enable_shared_from_this<DhtSearch>, protected std::map<std::unique_ptr<DhtNode>, std::shared_ptr<DhtSearch>, dht_compare_closer> {
public:
  using base_type = std::map<std::unique_ptr<DhtNode>, std::shared_ptr<DhtSearch>, dht_compare_closer>;

  // max_contacts: Number of closest potential contact nodes to keep.
  // max_announce: Number of closest nodes we actually announce to.
  static constexpr unsigned int max_contacts = 18;
  static constexpr unsigned int max_announce = 3;

  DhtSearch(DhtServer* server, const HashString& target);
  virtual ~DhtSearch();

  // Wrapper for iterators, allowing more convenient access to the key
  // and element values, which also makes it easier to change the container
  // without having to modify much code using iterators.
  template <typename T>
  struct accessor_wrapper : public T {
    accessor_wrapper() = default;
    accessor_wrapper(const T& itr) : T(itr) { }

    const auto&                       node() const     { return (**this).first; }
    const std::shared_ptr<DhtSearch>& search() const   { return (**this).second; }
  };

  using const_accessor = accessor_wrapper<base_type::const_iterator>;
  using accessor       = accessor_wrapper<base_type::iterator>;

  // Add a potential node to contact for the search.
  bool                 add_contact(const HashString& id, const sockaddr* sa);
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
  friend class torrent::DhtTransactionSearch;

  DhtServer*           server() const                    { return m_server; }

  void                 trim(bool is_final);

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

  DhtServer*           m_server;
  HashString           m_target;
};

} // namespace torrent::dht

#endif
