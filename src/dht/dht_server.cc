#include "config.h"

#include "dht/dht_server.h"

#include <algorithm>
#include <cerrno>

#include "manager.h"
#include "dht/dht_bucket.h"
#include "dht/dht_router.h"
#include "dht/dht_transaction.h"
#include "dht/transactions/dht_announce.h"
#include "torrent/exceptions.h"
#include "torrent/object.h"
#include "torrent/object_static_map.h"
#include "torrent/object_stream.h"
#include "torrent/net/socket_address.h"
#include "torrent/runtime/runtime.h"
#include "torrent/utils/algorithm.h"
#include "torrent/utils/log.h"
#include "tracker/tracker_dht.h"

#define LT_LOG_THIS(log_fmt, ...)                                       \
  lt_log_print(LOG_DHT_SERVER, "dht_server : " log_fmt, __VA_ARGS__);

namespace torrent {

// List of all possible keys we need/support in a DHT message.
// Unsupported keys we receive are dropped (ignored) while decoding.
// See torrent/object_static_map.h for how this works.
template <>
const DhtMessage::key_list_type DhtMessage::base_type::keys = {
  { key_a_id,       "a::id*S" },
  { key_a_infoHash, "a::info_hash*S" },
  { key_a_port,     "a::port", },
  { key_a_target,   "a::target*S" },
  { key_a_token,    "a::token*S" },

  { key_e_0,        "e[]*" },
  { key_e_1,        "e[]*" },

  { key_q,          "q*S" },

  { key_r_id,       "r::id*S" },
  { key_r_nodes,    "r::nodes*S" },
  { key_r_nodes6,   "r::nodes6*S" },
  { key_r_token,    "r::token*S" },
  { key_r_values,   "r::values*L" },

  { key_t,          "t*S" },
  { key_v,          "v*" },
  { key_y,          "y*S" },
};

DhtServer::DhtServer(DhtRouter* router)
  : m_router(router) {

  // UdpRouter only dispatches one- or two-byte transaction IDs and does not
  // distinguish queries from replies when a transaction key matches.
  m_udp_router.set_unknown_callback([this](auto id, auto sa, auto& buffer) {
    process_unknown_incoming(id, sa, buffer);
  });

  reset_statistics();
}

DhtServer::~DhtServer() {
  stop();
}

void
DhtServer::start(int port) {
  m_udp_router.open(m_router->family(), port);
  m_router->set_address(m_udp_router.socket_address());
}

void
DhtServer::stop() {
  if (!is_active())
    return;

  LT_LOG_THIS("stopping", 0);
  LT_LOG_THIS("searches : count:%zu", m_searches.size());

  clear_transactions();

  m_udp_router.close();

  m_network_up = false;
}

void
DhtServer::reset_statistics() {
  m_queries_received = 0;
  m_queries_sent     = 0;
  m_replies_received = 0;
  m_errors_received  = 0;
  m_errors_caught    = 0;
}

// Ping a node whose ID we know.
void
DhtServer::ping(const HashString& id, const sockaddr* sa) {
  if (m_transactions.size() >= max_transactions)
    return;

  if (!sa_is_inet(sa) && !sa_is_inet6(sa))
    return;

  // No point pinging a node that we're already contacting otherwise.
  auto itr = m_transactions.lower_bound(DhtTransaction::key(sa, 0));

  if (itr == m_transactions.end() || !DhtTransaction::key_match(itr->first, sa))
    add_transaction(std::unique_ptr<DhtTransaction>(new DhtTransactionPing(id, sa)), packet_prio_low);
}

// Contact nodes in given bucket and ask for their nodes closest to target.
void
DhtServer::find_node(const DhtBucket& contacts, const HashString& target) {
  auto search = std::make_shared<dht::DhtSearch>(this, target);
  search->add_contacts(contacts);

  auto n = search->get_contact();

  while (n != search->end()) {
    add_transaction(std::unique_ptr<DhtTransaction>(new DhtTransactionFindNode(search, n)), packet_prio_low);
    n = search->get_contact();
  }

  // This shouldn't happen, it means we had no contactable nodes at all.
  if (!search->start())
    return;
    // throw internal_error("DhtServer::find_node search start failed, no contactable nodes.");  ///////////////// TMP

  m_searches.insert(search);
}

void
DhtServer::announce(const DhtBucket& contacts, const HashString& infoHash, std::weak_ptr<TrackerDht> tracker, bool primary) {
  auto announce = std::make_shared<dht::DhtAnnounce>(this, infoHash, tracker, primary);
  announce->add_contacts(contacts);

  auto n = announce->get_contact();

  while (n != announce->end()) {
    add_transaction(std::unique_ptr<DhtTransaction>(new DhtTransactionGetPeers(announce, n)), packet_prio_high);
    n = announce->get_contact();
  }

  // This can only happen if all nodes we know are bad.
  if (!announce->start())
    return;
    // throw internal_error("DhtServer::announce search start failed, no contactable nodes.");  ///////////////// TMP

  m_searches.insert(announce);
  announce->update_status();
}

void
DhtServer::cancel_announce(const HashString& info_hash, std::weak_ptr<TrackerDht> tracker) {
  auto itr = m_transactions.begin();

  // TODO: Verify this removes us from m_searches.

  while (itr != m_transactions.end()) {
    if (itr->second->is_search() && itr->second->as_search()->search()->is_announce()) {
      auto announce = dynamic_cast<dht::DhtAnnounce*>(itr->second->as_search()->search().get());

      if (announce == nullptr)
        throw internal_error("DhtServer::cancel_announce dynamic_cast to DhtAnnounce failed.");

      bool tracker_match = !tracker.owner_before(announce->tracker()) && !announce->tracker().owner_before(tracker);

      if (announce->target() == info_hash && (tracker.expired() || tracker_match)) {
        itr = erase_transaction(itr);
        continue;
      }
    }

    ++itr;
  }
}

void
DhtServer::update() {
  // Reset this every 15 minutes. It'll get set back to true if we receive
  // any valid packets. This allows detecting when the entire network goes
  // down, and prevents all nodes from getting removed as unresponsive.
  m_network_up = false;
}

void
DhtServer::check_search_completed(std::shared_ptr<dht::DhtSearch> search) {
  if (!search->complete())
    return;

  // Removing search if has completed.

  auto itr = m_searches.find(search);

  if (itr == m_searches.end())
    return; // throw internal_error("DhtServer::mark_search_completed search not found.");

  // TODO: Verify we got ref_count == 2.

  m_searches.erase(itr);
}

void
DhtServer::check_search_trimming(std::shared_ptr<dht::DhtSearch> search) {
  // Make sure trimming search does not delete searches that have not completed.

  if (search->complete())
    return;

  // TODO: Should we erase if complete?

  auto itr = m_searches.lower_bound(search);

  if (itr != m_searches.end() && *itr == search)
    return;

  m_searches.insert(itr, search);
}

void
DhtServer::process_query(const HashString& id, const sockaddr* sa, const DhtMessage& msg) {
  m_queries_received++;
  m_network_up = true;

  raw_string query = msg[key_q].as_raw_string();

  // Construct reply.
  DhtMessage reply;

  if (query == raw_string::from_c_str("find_node"))
    create_find_node_response(msg, reply);

  else if (query == raw_string::from_c_str("get_peers"))
    create_get_peers_response(msg, sa, reply);

  else if (query == raw_string::from_c_str("announce_peer"))
    create_announce_peer_response(msg, sa, reply);

  else if (query != raw_string::from_c_str("ping"))
    throw dht::dht_error(dht_error_bad_method, "Unknown query type.");

  m_router->node_queried(id, sa);
  create_response(msg, sa, reply);
}

void
DhtServer::create_find_node_response(const DhtMessage& req, DhtMessage& reply) {
  raw_string target = req[key_a_target].as_raw_string();

  if (target.size() < HashString::size_data)
    throw dht::dht_error(dht_error_protocol, "target string too short");

  auto nodes_key = m_router->family() == AF_INET6 ? key_r_nodes6 : key_r_nodes;

  reply[nodes_key] = m_router->get_closest_nodes(*HashString::cast_from(target.data()));

  if (reply[nodes_key].as_raw_string().empty())
    throw dht::dht_error(dht_error_generic, "No nodes");
}

void
DhtServer::create_get_peers_response(const DhtMessage& req, const sockaddr* sa, DhtMessage& reply) {
  reply[key_r_token] = m_router->make_token(sa, reply.data_end);
  reply.data_end += reply[key_r_token].as_raw_string().size();

  raw_string info_hash_str = req[key_a_infoHash].as_raw_string();

  if (info_hash_str.size() < HashString::size_data)
    throw dht::dht_error(dht_error_protocol, "info hash too short");

  const HashString* info_hash = HashString::cast_from(info_hash_str.data());

  DhtTracker* tracker = m_router->get_tracker(*info_hash, false);

  // If we're not tracking or have no peers, send closest nodes.
  if (!tracker || tracker->empty()) {
    raw_string nodes = m_router->get_closest_nodes(*info_hash);

    if (nodes.empty())
      throw dht::dht_error(dht_error_generic, "No peers nor nodes");

    reply[m_router->family() == AF_INET6 ? key_r_nodes6 : key_r_nodes] = nodes;

  } else if (m_router->family() == AF_INET6) {
    reply[key_r_values] = tracker->get_peers6();

  } else {
    reply[key_r_values] = tracker->get_peers();
  }
}

void
DhtServer::create_announce_peer_response(const DhtMessage& req, const sockaddr* sa, [[maybe_unused]] DhtMessage& reply) {
  raw_string info_hash = req[key_a_infoHash].as_raw_string();

  if (info_hash.size() < HashString::size_data)
    throw dht::dht_error(dht_error_protocol, "info hash too short");

  if (!m_router->token_valid(req[key_a_token].as_raw_string(), sa))
    throw dht::dht_error(dht_error_protocol, "Token invalid.");

  if (!req[key_a_port].is_value())
    throw dht::dht_error(dht_error_protocol, "Port missing.");

  auto port = req[key_a_port].as_value();

  if (port <= 0 || port > 65535)
    throw dht::dht_error(dht_error_protocol, "Port invalid.");

  if (sa_is_inet(sa)) {
    DhtTracker* tracker = m_router->get_tracker(*HashString::cast_from(info_hash.data()), true);

    if (tracker == NULL)
      throw dht::dht_error(dht_error_generic, "Tracking too many info hashes");

    tracker->add_peer(reinterpret_cast<const sockaddr_in*>(sa)->sin_addr.s_addr, port);

  } else if (sa_is_inet6(sa)) {
    DhtTracker* tracker = m_router->get_tracker(*HashString::cast_from(info_hash.data()), true);

    if (tracker == NULL)
      throw dht::dht_error(dht_error_generic, "Tracking too many info hashes");

    tracker->add_peer6(reinterpret_cast<const sockaddr_in6*>(sa)->sin6_addr, port);

  } else {
    throw internal_error("DhtServer::create_announce_peer_response called with non-inet address.");
  }
}

bool
DhtServer::process_known_incoming(DhtTransaction::key_type key, dht::UdpRouter::buffer_type& buffer) {
  auto itr = m_transactions.find(key);

  if (itr == m_transactions.end())
    return false;

  DhtMessage        message;
  const HashString* node_id{};

  try {
    static_map_read_bencode(buffer.begin_ptr(), buffer.end_ptr(), message);
  } catch (const bencode_error& e) {
    create_error(message, itr->second->address(), dht_error_protocol, "Malformed packet: " + std::string(e.what()));
    return true;
  }

  if (!message[key_y].is_raw_string() || message[key_y].as_raw_string().size() != 1)
    return true;

  auto type = message[key_y].as_raw_string().data()[0];

  try {
    switch (type) {
    case 'r': {
      if (!message[key_r_id].is_raw_string())
        throw dht::dht_error(dht_error_protocol, "Invalid `id' type");

      auto& node_id_str = message[key_r_id].as_raw_string();

      if (node_id_str.size() < HashString::size_data)
        throw dht::dht_error(dht_error_protocol, "`id' value too short");

      node_id = HashString::cast_from(node_id_str.data());

      if (*node_id == m_router->id())
        throw dht::dht_error(dht_error_protocol, "Send your own ID, not mine");

      if (message[key_r_id].is_raw_string() && message[key_r_id].as_raw_string().size() >= HashString::size_data) {
        auto& id = message[key_r_id].as_raw_string();

        return process_response(itr, *HashString::cast_from(id.data()), message);
      }

      break;
    }
    case 'e':
      return process_error(itr);
    }

  } catch (const dht::dht_error& e) {
    if (node_id != nullptr)
      m_router->node_inactive(*node_id, itr->second->address());

    // Don't reply to potentially malicious probing packets.
    if (e.code() == dht_error_generic)
      create_error(message, itr->second->address(), e.code(), e.what());
  }

  return true;
}

void
DhtServer::process_unknown_incoming([[maybe_unused]] DhtTransaction::key_type key, const sockaddr* sa, dht::UdpRouter::buffer_type& buffer) {
  DhtMessage message;

  try {
    static_map_read_bencode(buffer.begin_ptr(), buffer.end_ptr(), message);

  } catch (const bencode_error& e) {
    create_error(message, sa, dht_error_protocol, "Malformed packet: " + std::string(e.what()));
    return;
  }

  try {
    if (!message[key_t].is_raw_string())
      throw dht::dht_error(dht_error_protocol, "No transaction ID");

    if (message[key_t].as_raw_string().size() > 20)
      throw dht::dht_error(dht_error_protocol, "Transaction ID length too long");

    if (!message[key_y].is_raw_string())
      throw dht::dht_error(dht_error_protocol, "No message type");

    if (message[key_y].as_raw_string().size() != 1)
      throw dht::dht_error(dht_error_bad_method, "Unsupported message type");

    auto type = message[key_y].as_raw_string().data()[0];

    if (type == 'r' || type == 'e')
      return; // ignore replies and errors for unknown transactions

    if (type != 'q')
      throw dht::dht_error(dht_error_bad_method, "Unknown message type.");

    if (!message[key_a_id].is_raw_string())
      throw dht::dht_error(dht_error_protocol, "Invalid `id' value");

    auto& id = message[key_a_id].as_raw_string();

    if (id.size() < HashString::size_data)
      throw dht::dht_error(dht_error_protocol, "`id' value too short");

    if (*HashString::cast_from(id.data()) == m_router->id())
      throw dht::dht_error(dht_error_protocol, "Send your own ID, not mine");

    process_query(*HashString::cast_from(id.data()), sa, message);

  } catch (const dht::dht_error& e) {
    if (e.code() == dht_error_generic)
      create_error(message, sa, e.code(), e.what());
  }
}

bool
DhtServer::process_response(transaction_itr itr, const HashString& id, const DhtMessage& response) {
  m_replies_received++;
  m_network_up = true;

  auto& transaction = itr->second;

  // If we contact a node but its ID is not the one we expect, ignore the reply.
  if ((id != transaction->id() && transaction->id() != torrent::DhtRouter::zero_id))
    return true;

  // Make sure transaction is erased even if an exception is thrown.
  try {
    switch (transaction->type()) {
      case DhtTransaction::DHT_FIND_NODE:
        // the get_peers walk of an announce: the peers a node on the way holds go to the tracker at once
        if (transaction->as_find_node()->search()->is_announce() && response[key_r_values].is_raw_list()) {
          auto announce = dynamic_cast<dht::DhtAnnounce*>(transaction->as_find_node()->search().get());

          if (announce != nullptr)
            announce->receive_peers(response[key_r_values].as_raw_list());
        }

        parse_find_node_reply(transaction->as_find_node(), response);
        break;

      case DhtTransaction::DHT_GET_PEERS:
        parse_get_peers_reply(transaction->as_get_peers(), response);
        break;

      // Nothing to do for DHT_PING and DHT_ANNOUNCE_PEER
      default:
        break;
    }

    // Mark node responsive only if all processing was successful, without errors.
    m_router->node_replied(id, itr->second->address());

  } catch (const dht::dht_error&) {
    m_router->node_inactive(id, itr->second->address());
    erase_transaction_skip_disconnect(itr);

    m_errors_caught++;
    return false;
  }

  erase_transaction_skip_disconnect(itr);
  return false;
}

bool
DhtServer::process_error(transaction_itr itr) {
  m_replies_received++;
  m_errors_received++;

  m_network_up = true;

  // Don't mark node as good (because it replied) or bad (because it returned an error).
  // If it consistently returns errors for valid queries it's probably broken.  But a
  // few error messages are acceptable. So we do nothing and pretend the query never happened.

  erase_transaction_skip_disconnect(itr);
  return false;
}

void
DhtServer::parse_find_node_reply(DhtTransactionSearch* transaction, const DhtMessage& response) {
  transaction->complete(true);

  add_search_contacts(transaction, response);
  find_node_next(transaction);
}

void
DhtServer::add_search_contacts(DhtTransactionSearch* transaction, const DhtMessage& response) {
  if (sizeof(const compact_node_info) != 26)
    throw internal_error("DhtServer::add_search_contacts(...) bad struct size.");

  if (m_router->family() == AF_INET6) {
    if (response[key_r_nodes6].is_raw_string()) {
      raw_string nodes = response[key_r_nodes6].as_raw_string();

      for (auto itr = nodes.data(); itr + 38 <= nodes.data() + nodes.size(); itr += 38) {
        const HashString* node_id = HashString::cast_from(itr);

        if (*node_id == m_router->id())
          continue;

        auto sin6 = sin6_make_n(itr + 20, *reinterpret_cast<const uint16_t*>(itr + 36));

        // a v4-mapped entry is an IPv4 node in disguise: not this table's
        if (sin6_is_v4mapped(sin6.get()) || sin6_is_any(sin6.get()) || sin6->sin6_port == 0)
          continue;

        transaction->search()->add_contact(*node_id, sa_from_in6(std::move(sin6)).get());
      }
    }

  } else {
    if (response[key_r_nodes].is_raw_string()) {
      raw_string nodes = response[key_r_nodes].as_raw_string();

      for (auto itr = nodes.data(); itr + 26 <= nodes.data() + nodes.size(); itr += 26) {
        const HashString* node_id = HashString::cast_from(itr);

        if (*node_id == m_router->id())
          continue;

        auto sin = sin_make_n(*reinterpret_cast<const uint32_t*>(itr + 20), *reinterpret_cast<const uint16_t*>(itr + 24));

        if (sin_is_any(sin.get()) || sin->sin_port == 0)
          continue;

        transaction->search()->add_contact(*node_id, sa_from_in(std::move(sin)).get());
      }
    }
  }
}

void
DhtServer::parse_get_peers_reply(DhtTransactionGetPeers* transaction, const DhtMessage& response) {
  auto announce = dynamic_cast<dht::DhtAnnounce*>(transaction->as_search()->search().get());

  if (announce == nullptr)
    throw internal_error("DhtServer::parse_get_peers_reply dynamic_cast to DhtAnnounce failed.");

  transaction->complete(true);

  if (response[key_r_values].is_raw_list())
    announce->receive_peers(response[key_r_values].as_raw_list());

  // Restrict the length of tokens. We echo them back in announce_peer, and the query has to fit in
  // a single packet.
  //
  // A reply with a longer token still carries nodes for the walk.
  if (response[key_r_token].is_raw_string() && response[key_r_token].as_raw_string().size() <= 64)
    announce->set_token(transaction->id(), response[key_r_token].as_raw_string());

  add_search_contacts(transaction, response);

  find_node_next(transaction);
}

void
DhtServer::find_node_next(DhtTransactionSearch* transaction) {
  auto node = transaction->search()->get_contact();

  while (node != transaction->search()->end()) {
    if (transaction->search()->is_announce())
      add_transaction(std::unique_ptr<DhtTransaction>(new DhtTransactionGetPeers(transaction->search(), node)), packet_prio_high);
    else
      add_transaction(std::unique_ptr<DhtTransaction>(new DhtTransactionFindNode(transaction->search(), node)), packet_prio_low);

    node = transaction->search()->get_contact();
  }

  if (!transaction->search()->is_announce())
    return;

  auto announce = dynamic_cast<dht::DhtAnnounce*>(transaction->search().get());

  if (announce == nullptr)
    throw internal_error("DhtServer::find_node_next dynamic_cast to DhtAnnounce failed.");

  if (announce->complete()) {
    // We have found the 8 closest nodes to the info hash, announce to them.
    for (const auto& [node, token] : announce->start_announce())
      add_transaction(std::unique_ptr<DhtTransaction>(new DhtTransactionAnnouncePeer(node->id(), node->address(), announce->target(), token)), packet_prio_high);
  }

  announce->update_status();
}

std::shared_ptr<DhtMessage>
DhtServer::create_query(transaction_itr itr, int tID) {
  if (itr->second->id() == m_router->id())
    throw internal_error("DhtServer::create_query trying to send to itself.");

  auto message = std::make_shared<DhtMessage>();
  auto& query = *message;

  auto& transaction = itr->second;

  query[key_t] = std::string(1, tID);
  query[key_q] = raw_string::from_c_str(queries[transaction->type()]);
  query[key_y] = raw_bencode::from_c_str("1:q");
  query[key_v] = raw_bencode("4:" PEER_VERSION, 6);
  query[key_a_id] = m_router->id().str();

  switch (transaction->type()) {
    case DhtTransaction::DHT_PING:
      // nothing to do
      break;

    case DhtTransaction::DHT_FIND_NODE:
      // A peer search walks with get_peers (BEP 5): every node on the way may hold the info-hash's peers and
      // answers with them beside the closer nodes; a find_node walk only asked the last few nodes for peers.
      if (transaction->as_find_node()->search()->is_announce()) {
        query[key_q] = raw_string::from_c_str("get_peers");
        query[key_a_infoHash] = transaction->as_find_node()->search()->target().str();
      } else {
        query[key_a_target] = transaction->as_find_node()->search()->target().str();
      }
      break;

    case DhtTransaction::DHT_GET_PEERS:
      query[key_a_infoHash] = transaction->as_get_peers()->search()->target().str();
      break;

    case DhtTransaction::DHT_ANNOUNCE_PEER:
      query[key_a_infoHash] = transaction->as_announce_peer()->info_hash().str();
      query[key_a_token]    = transaction->as_announce_peer()->token().as_string();
      query[key_a_port]     = runtime::listen_port();
      break;
  }

  return message;
}

void
DhtServer::create_response(const DhtMessage& req, const sockaddr* sa, DhtMessage& reply) {
  reply[key_r_id] = m_router->id_raw_string();
  reply[key_t] = req[key_t];
  reply[key_y] = raw_bencode::from_c_str("1:r");
  reply[key_v] = raw_bencode("4:" PEER_VERSION, 6);

  send_reply(sa, reply, packet_prio_reply);
}

void
DhtServer::create_error(const DhtMessage& req, const sockaddr* sa, int num, const char* msg) {
  DhtMessage error;

  if (req[key_t].is_raw_string() && req[key_t].as_raw_string().size() < 67)
    error[key_t] = req[key_t];

  error[key_y] = raw_bencode::from_c_str("1:e");
  error[key_v] = raw_bencode("4:" PEER_VERSION, 6);
  error[key_e_0] = num;
  error[key_e_1] = raw_string::from_c_str(msg);

  send_reply(sa, error, packet_prio_reply);
}

void
DhtServer::send_reply(const sockaddr* sa, DhtMessage& reply, [[maybe_unused]] int priority) {
  auto start_id = m_next_reply_id;

  do {
    if (++m_next_reply_id > 0xffff || m_next_reply_id == 0)
      m_next_reply_id = 1;

    if (m_next_reply_id == start_id)
      return;

  } while (m_udp_router.has_connection_id(m_next_reply_id));

  auto message = std::make_shared<dht::UdpRouter::buffer_type>();
  auto result  = static_map_write_bencode_c(object_write_to_buffer, nullptr,
                                            object_buffer_t(message->begin_ptr(), message->begin_ptr() + 1452 + 1),
                                            reply);

  if (result.second - result.first > 1452)
    throw internal_error("DhtServer::send_reply reply too large for UDP packet.");

  message->set_end_ptr(result.second);

  // UdpRouter has no one-shot or prioritized send; replies are retried as connections.
  m_udp_router.connect(m_next_reply_id, sa_copy(sa), dht::UdpRouter::connection_params{
      [message = std::move(message)](auto, auto& buffer) {
        buffer.write_range(message->begin(), message->end());
      },
      nullptr,
      [](auto, int, int) {},
      [this](auto) { m_queries_sent++; }
    });
}

int
DhtServer::add_transaction(std::shared_ptr<DhtTransaction> transaction, [[maybe_unused]] int priority) {
  if (!is_active()) {
    LT_LOG_THIS("cannot send query : UDP router is closed", 0);
    return -1;
  }

  if (!sa_is_inet(transaction->address()) && !sa_is_inet6(transaction->address()))
    return -1;

  const auto address_key = DhtTransaction::key(transaction->address(), 0);

  if (address_key == 0)
    throw internal_error("DhtServer::add_transaction address hash is zero.");

  // Try random transaction ID. This is to make it less likely that we reuse
  // a transaction ID from an earlier transaction which timed out and we forgot
  // about it, so that if the node replies after the timeout it's less likely
  // that we match the reply to the wrong transaction.
  //
  // If there's an existing transaction with the random ID we search for the next
  // unused one. Since normally only one or two transactions will be active per
  // node, a collision is extremely unlikely, and a linear search for the first
  // open one is the most efficient.

  auto [key_id, lower_bound, found] = utils::find_available_key(m_transactions,
                                                                address_key | (1 + random() % 127),
                                                                address_key | 1,
                                                                address_key | 127);

  if (!found)
    return -1;

  // We know where to insert it, so pass that as hint.
  auto itr = m_transactions.insert(lower_bound, std::make_pair(key_id, transaction));

  try {
    auto query = create_query(itr, key_id & 0xff);

    auto message = std::make_shared<dht::UdpRouter::buffer_type>();
    auto result  = static_map_write_bencode_c(object_write_to_buffer, nullptr,
                                              object_buffer_t(message->begin_ptr(), message->begin_ptr() + 1452 + 1),
                                              *query);

    if (result.second - result.first > 1452)
      throw internal_error("DhtServer::send_reply reply too large for UDP packet.");

    message->set_end_ptr(result.second);

    auto params = dht::UdpRouter::connection_params{
      [message = std::move(message)](auto, auto& buffer) {
        buffer.write_range(message->begin(), message->end());
      },
      [this](auto key, auto& buffer) {
        return process_known_incoming(key, buffer);
      },
      [this](auto key, int, int) {
        auto itr = m_transactions.find(key);

        if (itr != m_transactions.end())
          failed_transaction(itr, false, true);
      },
      [this](auto key) {
        ++m_queries_sent;

        auto itr = m_transactions.find(key);

        if (itr != m_transactions.end())
          itr->second->set_packet_sent();
      },
    };

    // UdpRouter has no priority or quick-timeout callback for stalled searches.
    m_udp_router.connect(itr->first, transaction->address_ptr(), params);

  } catch (const std::exception&) {
    m_udp_router.try_disconnect(key_id);

    auto current = m_transactions.find(key_id);

    if (current != m_transactions.end())
      m_transactions.erase(current);

    throw;
  }

  return key_id & 0xff;
}

// Transaction received no reply and timed out. Mark node as bad and remove
// transaction (except if it was only the quick timeout).
DhtServer::transaction_itr
DhtServer::failed_transaction(transaction_itr itr, bool quick, bool router_disconnected) {
  auto transaction = itr->second;

  // If it was a known node, remember that it didn't reply, unless the transaction
  // is only stalled (had quick timeout, but not full timeout).  Also if the
  // transaction has not been sent, the packet may have been queued due to
  // throttling, so don't blame the remote node for not replying.
  // Finally, if we haven't received anything whatsoever so far, assume the entire
  // network is down and so we can't blame the node either.
  if (!quick && m_network_up && transaction->packet_sent() && transaction->id() != torrent::DhtRouter::zero_id)
    m_router->node_inactive(transaction->id(), transaction->address());

  if (transaction->type() == DhtTransaction::DHT_FIND_NODE || transaction->type() == DhtTransaction::DHT_GET_PEERS) {
    if (quick)
      transaction->as_search()->set_stalled();
    else
      transaction->as_search()->complete(false);

    try {
      find_node_next(transaction->as_search());

    } catch (const std::exception&) {
      if (!quick) {
        if (router_disconnected)
          erase_transaction_skip_disconnect(itr);
        else
          erase_transaction(itr);
      }

      throw;
    }
  }

  // don't actually delete the transaction until the final timeout
  if (quick)
    return ++itr;

  return router_disconnected ? erase_transaction_skip_disconnect(itr) : erase_transaction(itr);
}

DhtServer::transaction_itr
DhtServer::erase_transaction(transaction_itr itr) {
  m_udp_router.disconnect(itr->first);
  return erase_transaction_skip_disconnect(itr);
}

DhtServer::transaction_itr
DhtServer::erase_transaction_skip_disconnect(transaction_itr itr) {
  return m_transactions.erase(itr);
}

void
DhtServer::clear_transactions() {
  while (!m_transactions.empty())
    erase_transaction(m_transactions.begin());
}

} // namespace torrent
