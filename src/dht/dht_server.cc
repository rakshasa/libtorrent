#include "config.h"

#include "dht/dht_server.h"

#include <algorithm>
#include <cstdio>

#include "dht/dht_bucket.h"
#include "dht/dht_router.h"
#include "dht/dht_transaction.h"
#include "manager.h"
#include "torrent/connection_manager.h"
#include "torrent/exceptions.h"
#include "torrent/object.h"
#include "torrent/object_static_map.h"
#include "torrent/object_stream.h"
#include "torrent/net/fd.h"
#include "torrent/net/poll.h"
#include "torrent/net/socket_address.h"
#include "torrent/net/network_config.h"
#include "torrent/net/network_manager.h"
#include "torrent/utils/log.h"
#include "tracker/tracker_dht.h"

#define LT_LOG_THIS(log_fmt, ...)                                       \
  lt_log_print_subsystem(torrent::LOG_DHT_SERVER, "dht_server", log_fmt, __VA_ARGS__);

namespace {
// Error in DHT protocol, avoids std::string ctor from communication_error
class dht_error : public torrent::network_error {
public:
  dht_error(int code, const char* message) :
      m_message(message), m_code(code) {}

  int         code() const noexcept { return m_code; }
  const char* what() const noexcept override { return m_message; }

private:
  const char* m_message;
  int         m_code;
};

} // namespace

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
  { key_r_token,    "r::token*S" },
  { key_r_values,   "r::values*L" },

  { key_t,          "t*S" },
  { key_v,          "v*" },
  { key_y,          "y*S" },
};

DhtServer::DhtServer(DhtRouter* router)
  : m_router(router) {

  m_fileDesc = -1;
  reset_statistics();

  // Reserve a socket for the DHT server, even though we don't
  // actually open it until the server is started, which may not
  // happen until the first non-private torrent is started.
  manager->connection_manager()->inc_socket_count();

  m_task_timeout.slot() = [this] { receive_timeout(); };
}

DhtServer::~DhtServer() {
  stop();

  manager->connection_manager()->dec_socket_count();
}

void
DhtServer::start(int port) {
  auto [bind_inet_address, bind_inet6_address] = config::network_config()->bind_addresses_or_null();

  if (bind_inet_address == nullptr)
    throw resource_error("no valid bind address for DHT server");

  sa_unique_ptr bind_address;

  switch (bind_inet_address->sa_family) {
  case AF_INET:
    bind_address = sa_copy(bind_inet_address.get());
    break;
  case AF_UNSPEC:
    bind_address = sa_make_inet_any();
    break;
  default:
    throw resource_error("invalid address family for DHT server");
  }

  m_router->set_address(bind_inet_address.get());
  sap_set_port(bind_address, port);

  try {
    LT_LOG_THIS("starting server : %s", sap_pretty_str(bind_address).c_str());

    fd_flags open_flags = fd_flag_datagram | fd_flag_nonblock | fd_flag_reuse_address;

    if (bind_address->sa_family == AF_INET)
      open_flags |= fd_flag_v4;

    m_fileDesc = fd_open(open_flags);

    if (m_fileDesc == -1)
      throw resource_error("could not open datagram socket : " + std::string(strerror(errno)));

    // Figure out how to bind to both inet and inet6.
    if (!fd_bind(m_fileDesc, bind_address.get()))
      throw resource_error("could not bind datagram socket : " + std::string(strerror(errno)));

  } catch (const torrent::base_error&) {
    fd_close(m_fileDesc);
    m_fileDesc = -1;
    throw;
  }

  this_thread::poll()->open(this);
  this_thread::poll()->insert_read(this);
  this_thread::poll()->insert_error(this);
}

void
DhtServer::stop() {
  if (!is_active())
    return;

  LT_LOG_THIS("stopping", 0);

  clear_transactions();

  this_thread::scheduler()->erase(&m_task_timeout);
  this_thread::poll()->remove_and_close(this);

  fd_close(m_fileDesc);
  m_fileDesc = -1;

  m_networkUp = false;
}

void
DhtServer::reset_statistics() {
  m_queriesReceived = 0;
  m_queriesSent = 0;
  m_repliesReceived = 0;
  m_errorsReceived = 0;
  m_errorsCaught = 0;
}

// Ping a node whose ID we know.
void
DhtServer::ping(const HashString& id, const sockaddr* sa) {
  // No point pinging a node that we're already contacting otherwise.
  auto itr = m_transactions.lower_bound(DhtTransaction::key(sa, 0));

  if (itr == m_transactions.end() || !DhtTransaction::key_match(itr->first, sa))
    add_transaction(std::unique_ptr<DhtTransaction>(new DhtTransactionPing(id, sa)), packet_prio_low);
}

// Contact nodes in given bucket and ask for their nodes closest to target.
void
DhtServer::find_node(const DhtBucket& contacts, const HashString& target) {
  auto search = new DhtSearch(target, contacts);

  auto n = search->get_contact();
  while (n != search->end()) {
    add_transaction(std::unique_ptr<DhtTransaction>(new DhtTransactionFindNode(n)), packet_prio_low);
    n = search->get_contact();
  }

  // This shouldn't happen, it means we had no contactable nodes at all.
  if (!search->start())
    delete search;
}

void
DhtServer::announce(const DhtBucket& contacts, const HashString& infoHash, TrackerDht* tracker) {
  auto announce = new DhtAnnounce(infoHash, tracker, contacts);
  auto n        = announce->get_contact();

  while (n != announce->end()) {
    add_transaction(std::unique_ptr<DhtTransaction>(new DhtTransactionFindNode(n)), packet_prio_high);
    n = announce->get_contact();
  }

  // This can only happen if all nodes we know are bad.
  if (!announce->start())
    delete announce;
  else
    announce->update_status();
}

void
DhtServer::cancel_announce(const HashString* info_hash, const TrackerDht* tracker) {
  auto itr = m_transactions.begin();

  while (itr != m_transactions.end()) {
    if (itr->second->is_search() && itr->second->as_search()->search()->is_announce()) {
      auto announce = static_cast<DhtAnnounce*>(itr->second->as_search()->search());

      if ((info_hash == nullptr || announce->target() == *info_hash) && (tracker == nullptr || announce->tracker() == tracker)) {
        drop_packet(itr->second->packet());
        m_transactions.erase(itr++);
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
  m_networkUp = false;
}

void
DhtServer::process_query(const HashString& id, const sockaddr* sa, const DhtMessage& msg) {
  m_queriesReceived++;
  m_networkUp = true;

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
    throw dht_error(dht_error_bad_method, "Unknown query type.");

  m_router->node_queried(id, sa);
  create_response(msg, sa, reply);
}

void
DhtServer::create_find_node_response(const DhtMessage& req, DhtMessage& reply) {
  raw_string target = req[key_a_target].as_raw_string();

  if (target.size() < HashString::size_data)
    throw dht_error(dht_error_protocol, "target string too short");

  reply[key_r_nodes] = m_router->get_closest_nodes(*HashString::cast_from(target.data()));

  if (reply[key_r_nodes].as_raw_string().empty())
    throw dht_error(dht_error_generic, "No nodes");
}

void
DhtServer::create_get_peers_response(const DhtMessage& req, const sockaddr* sa, DhtMessage& reply) {
  reply[key_r_token] = m_router->make_token(sa, reply.data_end);
  reply.data_end += reply[key_r_token].as_raw_string().size();

  raw_string info_hash_str = req[key_a_infoHash].as_raw_string();

  if (info_hash_str.size() < HashString::size_data)
    throw dht_error(dht_error_protocol, "info hash too short");

  const HashString* info_hash = HashString::cast_from(info_hash_str.data());

  DhtTracker* tracker = m_router->get_tracker(*info_hash, false);

  // If we're not tracking or have no peers, send closest nodes.
  if (!tracker || tracker->empty()) {
    raw_string nodes = m_router->get_closest_nodes(*info_hash);

    if (nodes.empty())
      throw dht_error(dht_error_generic, "No peers nor nodes");

    reply[key_r_nodes] = nodes;

  } else {
    reply[key_r_values] = tracker->get_peers();
  }
}

void
DhtServer::create_announce_peer_response(const DhtMessage& req, const sockaddr* sa, [[maybe_unused]] DhtMessage& reply) {
  raw_string info_hash = req[key_a_infoHash].as_raw_string();

  if (info_hash.size() < HashString::size_data)
    throw dht_error(dht_error_protocol, "info hash too short");

  if (!m_router->token_valid(req[key_a_token].as_raw_string(), sa))
    throw dht_error(dht_error_protocol, "Token invalid.");

  if (!sa_is_inet(sa))
    throw internal_error("DhtServer::create_announce_peer_response called with non-inet address.");

  DhtTracker* tracker = m_router->get_tracker(*HashString::cast_from(info_hash.data()), true);

  tracker->add_peer(reinterpret_cast<const sockaddr_in*>(sa)->sin_addr.s_addr, req[key_a_port].as_value());
}

void
DhtServer::process_response(const HashString& id, const sockaddr* sa, const DhtMessage& response) {
  int  transactionId = static_cast<unsigned char>(response[key_t].as_raw_string().data()[0]);
  auto itr = m_transactions.find(DhtTransaction::key(sa, transactionId));

  // Response to a transaction we don't have in our table. At this point it's
  // impossible to tell whether it used to be a valid transaction but timed out
  // the node did not return the ID we sent it, or it returned it with a
  // different address than we sent it o. Best we can do is ignore the reply,
  // since the protocol doesn't call for returning errors in responses.
  if (itr == m_transactions.end())
    return;

  m_repliesReceived++;
  m_networkUp = true;

  // Make sure transaction is erased even if an exception is thrown.
  try {
    auto& transaction = itr->second;

#ifdef USE_EXTRA_DEBUG
    if (DhtTransaction::key(sa, transactionId) != transaction->key(transactionId))
      throw internal_error("DhtServer::process_response key mismatch.");
#endif

    // If we contact a node but its ID is not the one we expect, ignore the reply
    // to prevent interference from rogue nodes.
    if ((id != transaction->id() && transaction->id() != torrent::DhtRouter::zero_id))
      return;

    switch (transaction->type()) {
      case DhtTransaction::DHT_FIND_NODE:
        parse_find_node_reply(transaction->as_find_node(), response[key_r_nodes].as_raw_string());
        break;

      case DhtTransaction::DHT_GET_PEERS:
        parse_get_peers_reply(transaction->as_get_peers(), response);
        break;

      // Nothing to do for DHT_PING and DHT_ANNOUNCE_PEER
      default:
        break;
    }

    // Mark node responsive only if all processing was successful, without errors.
    m_router->node_replied(id, sa);

  } catch (const std::exception&) {
    drop_packet(itr->second->packet());
    m_transactions.erase(itr);

    m_errorsCaught++;
    throw;
  }

  drop_packet(itr->second->packet());
  m_transactions.erase(itr);
}

void
DhtServer::process_error(const sockaddr* sa, const DhtMessage& error) {
  int  transactionId = static_cast<unsigned char>(error[key_t].as_raw_string().data()[0]);
  auto itr = m_transactions.find(DhtTransaction::key(sa, transactionId));

  if (itr == m_transactions.end())
    return;

  m_repliesReceived++;
  m_errorsReceived++;
  m_networkUp = true;

  // Don't mark node as good (because it replied) or bad (because it returned an error).
  // If it consistently returns errors for valid queries it's probably broken.  But a
  // few error messages are acceptable. So we do nothing and pretend the query never happened.

  drop_packet(itr->second->packet());
  m_transactions.erase(itr);
}

void
DhtServer::parse_find_node_reply(DhtTransactionSearch* transaction, raw_string nodes) {
  transaction->complete(true);

  if (sizeof(const compact_node_info) != 26)
    throw internal_error("DhtServer::parse_find_node_reply(...) bad struct size.");

  node_info_list list;
  std::copy(reinterpret_cast<const compact_node_info*>(nodes.data()),
            reinterpret_cast<const compact_node_info*>(nodes.data() + nodes.size() - nodes.size() % sizeof(compact_node_info)),
            std::back_inserter(list));

  for (auto& node : list) {
    if (node.id() != m_router->id())
      transaction->search()->add_contact(node.id(), sa_make_inet_n(node._addr.addr, node._addr.port).get());
  }

  find_node_next(transaction);
}

void
DhtServer::parse_get_peers_reply(DhtTransactionGetPeers* transaction, const DhtMessage& response) {
  auto announce = static_cast<DhtAnnounce*>(transaction->as_search()->search());

  transaction->complete(true);

  if (response[key_r_values].is_raw_list())
    announce->receive_peers(response[key_r_values].as_raw_list());

  if (response[key_r_token].is_raw_string())
    add_transaction(std::unique_ptr<DhtTransaction>(new DhtTransactionAnnouncePeer(transaction->id(),
                                                                                   transaction->address(),
                                                                                   announce->target(),
                                                                                   response[key_r_token].as_raw_string())),
                    packet_prio_low);

  announce->update_status();
}

void
DhtServer::find_node_next(DhtTransactionSearch* transaction) {
  int priority = packet_prio_low;

  if (transaction->search()->is_announce())
    priority = packet_prio_high;

  auto node = transaction->search()->get_contact();

  while (node != transaction->search()->end()) {
    add_transaction(std::unique_ptr<DhtTransaction>(new DhtTransactionFindNode(node)), priority);
    node = transaction->search()->get_contact();
  }

  if (!transaction->search()->is_announce())
    return;

  auto announce = static_cast<DhtAnnounce*>(transaction->search());

  if (announce->complete()) {
    // We have found the 8 closest nodes to the info hash. Retrieve peers
    // from them and announce to them.
    for (node = announce->start_announce(); node != announce->end(); ++node)
      add_transaction(std::unique_ptr<DhtTransaction>(new DhtTransactionGetPeers(node)), packet_prio_high);
  }

  announce->update_status();
}

void
DhtServer::add_packet(std::shared_ptr<DhtTransactionPacket> packet, int priority) {
  switch (priority) {
    // High priority packets are for important queries, and quite small.
    // They're added to front of high priority queue and thus will be the
    // next packets sent.
    case packet_prio_high:
      m_highQueue.push_front(std::move(packet));
      break;

    // Low priority query packets are added to the back of the high priority
    // queue and will be sent when all high priority packets have been transmitted.
    case packet_prio_low:
      m_highQueue.push_back(std::move(packet));
      break;

    // Reply packets will be processed after all of our own packets have been send.
    case packet_prio_reply:
      m_lowQueue.push_back(std::move(packet));
      break;

    default:
      throw internal_error("DhtServer::add_packet called with invalid priority.");
  }
}

void
DhtServer::drop_packet(DhtTransactionPacket* packet) {
  m_highQueue.erase(std::remove_if(m_highQueue.begin(), m_highQueue.end(), [packet](auto& p) { return p.get() == packet; }),
                    m_highQueue.end());
  m_lowQueue.erase(std::remove_if(m_lowQueue.begin(), m_lowQueue.end(), [packet](auto& p) { return p.get() == packet; }),
                   m_lowQueue.end());

  if (m_highQueue.empty() && m_lowQueue.empty())
    this_thread::poll()->remove_write(this);
}

void
DhtServer::create_query(transaction_itr itr, int tID, [[maybe_unused]] const sockaddr* sa, int priority) {
  if (itr->second->id() == m_router->id())
    throw internal_error("DhtServer::create_query trying to send to itself.");

  DhtMessage query;

  // Transaction ID is a bencode string.
  query[key_t] = raw_bencode(query.data_end, 3);
  *query.data_end++ = '1';
  *query.data_end++ = ':';
  *query.data_end++ = tID;

  auto& transaction = itr->second;

  query[key_q] = raw_string::from_c_str(queries[transaction->type()]);
  query[key_y] = raw_bencode::from_c_str("1:q");
  query[key_v] = raw_bencode("4:" PEER_VERSION, 6);
  query[key_a_id] = m_router->id_raw_string();

  switch (transaction->type()) {
    case DhtTransaction::DHT_PING:
      // nothing to do
      break;

    case DhtTransaction::DHT_FIND_NODE:
      query[key_a_target] = transaction->as_find_node()->search()->target_raw_string();
      break;

    case DhtTransaction::DHT_GET_PEERS:
      query[key_a_infoHash] = transaction->as_get_peers()->search()->target_raw_string();
      break;

    case DhtTransaction::DHT_ANNOUNCE_PEER:
      query[key_a_infoHash] = transaction->as_announce_peer()->info_hash_raw_string();
      query[key_a_token] = transaction->as_announce_peer()->token();
      query[key_a_port] = runtime::network_manager()->listen_port();
      break;
  }

  auto packet = std::make_shared<DhtTransactionPacket>(transaction->address(), query, tID, transaction);

  transaction->set_packet(packet);
  add_packet(packet, priority);

  m_queriesSent++;
}

void
DhtServer::create_response(const DhtMessage& req, const sockaddr* sa, DhtMessage& reply) {
  reply[key_r_id] = m_router->id_raw_string();
  reply[key_t] = req[key_t];
  reply[key_y] = raw_bencode::from_c_str("1:r");
  reply[key_v] = raw_bencode("4:" PEER_VERSION, 6);

  add_packet(std::make_shared<DhtTransactionPacket>(sa, reply), packet_prio_reply);
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

  add_packet(std::make_shared<DhtTransactionPacket>(sa, error), packet_prio_reply);
}

int
DhtServer::add_transaction(std::shared_ptr<DhtTransaction> transaction, int priority) {
  // Try random transaction ID. This is to make it less likely that we reuse
  // a transaction ID from an earlier transaction which timed out and we forgot
  // about it, so that if the node replies after the timeout it's less likely
  // that we match the reply to the wrong transaction.
  //
  // If there's an existing transaction with the random ID we search for the next
  // unused one. Since normally only one or two transactions will be active per
  // node, a collision is extremely unlikely, and a linear search for the first
  // open one is the most efficient.
  unsigned int rnd = static_cast<uint8_t>(random());
  unsigned int id = rnd;

  auto insertItr = m_transactions.lower_bound(transaction->key(rnd));

  // If key matches, keep trying successive IDs.
  while (insertItr != m_transactions.end() && insertItr->first == transaction->key(id)) {
    ++insertItr;
    id = static_cast<uint8_t>(id + 1);

    // Give up after trying all possible IDs. This should never happen.
    if (id == rnd)
      return -1;

    // Transaction ID wrapped around, reset iterator.
    if (id == 0)
      insertItr = m_transactions.lower_bound(transaction->key(id));
  }

  // We know where to insert it, so pass that as hint.
  insertItr = m_transactions.insert(insertItr, std::make_pair(transaction->key(id), transaction));

  create_query(insertItr, id, transaction->address(), priority);
  start_write();

  return id;
}

// Transaction received no reply and timed out. Mark node as bad and remove
// transaction (except if it was only the quick timeout).
DhtServer::transaction_itr
DhtServer::failed_transaction(transaction_itr itr, bool quick) {
  auto transaction = itr->second;

  // If it was a known node, remember that it didn't reply, unless the transaction
  // is only stalled (had quick timeout, but not full timeout).  Also if the
  // transaction still has an associated packet, the packet never got sent due to
  // throttling, so don't blame the remote node for not replying.
  // Finally, if we haven't received anything whatsoever so far, assume the entire
  // network is down and so we can't blame the node either.
  if (!quick && m_networkUp && transaction->packet() == NULL && transaction->id() != torrent::DhtRouter::zero_id)
    m_router->node_inactive(transaction->id(), transaction->address());

  if (transaction->type() == DhtTransaction::DHT_FIND_NODE) {
    if (quick)
      transaction->as_find_node()->set_stalled();
    else
      transaction->as_find_node()->complete(false);

    try {
      find_node_next(transaction->as_find_node());

    } catch (const std::exception&) {
      if (!quick) {
        drop_packet(transaction->packet());
        m_transactions.erase(itr);
      }

      throw;
    }
  }

  if (quick) {
    return ++itr;         // don't actually delete the transaction until the final timeout

  } else {
    drop_packet(transaction->packet());
    m_transactions.erase(itr++);
    return itr;
  }
}

void
DhtServer::clear_transactions() {
  for (auto& transaction : m_transactions)
    drop_packet(transaction.second->packet());

  m_transactions.clear();
}

void
DhtServer::event_read() {
  while (true) {
    Object request;
    int type = '?';
    DhtMessage message;
    const HashString* nodeId = NULL;

    sockaddr_in6 sa_raw{};
    sockaddr* sa = reinterpret_cast<sockaddr*>(&sa_raw);

    try {
      char buffer[2048];
      int32_t read = read_datagram_sa(buffer, sizeof(buffer), sa, sizeof(sa_raw));

      if (read < 0)
        break;

      // We can currently only process mapped-IPv4 addresses, not real IPv6.
      // Translate them to an af_inet socket_address.
      if (sa_is_v4mapped(sa)) {
        auto sa_unmapped = sin_from_v4mapped_in6(&sa_raw);
        *reinterpret_cast<sockaddr_in*>(&sa_raw) = *sa_unmapped.get();
      }

      if (sa->sa_family != AF_INET)
        continue;

      // If it's not a valid bencode dictionary at all, it's probably not a DHT
      // packet at all, so we don't throw an error to prevent bounce loops.
      try {
        static_map_read_bencode(buffer, buffer + read, message);
      } catch (const bencode_error&) {
        continue;
      }

      if (!message[key_t].is_raw_string())
        throw dht_error(dht_error_protocol, "No transaction ID");

      // Restrict the length of Transaction IDs. We echo them in our replies.
      if(message[key_t].as_raw_string().size() > 20) {
		  throw dht_error(dht_error_protocol, "Transaction ID length too long");
      }

      if (!message[key_y].is_raw_string())
        throw dht_error(dht_error_protocol, "No message type");

      if (message[key_y].as_raw_string().size() != 1)
        throw dht_error(dht_error_bad_method, "Unsupported message type");

      type = message[key_y].as_raw_string().data()[0];

      // Queries and replies have node ID in different dictionaries.
      if (type == 'r' || type == 'q') {
        if (!message[type == 'q' ? key_a_id : key_r_id].is_raw_string())
          throw dht_error(dht_error_protocol, "Invalid `id' value");

        raw_string nodeIdStr = message[type == 'q' ? key_a_id : key_r_id].as_raw_string();

        if (nodeIdStr.size() < HashString::size_data)
          throw dht_error(dht_error_protocol, "`id' value too short");

        nodeId = HashString::cast_from(nodeIdStr.data());
      }

      // Sanity check the returned transaction ID.
      if ((type == 'r' || type == 'e') &&
          (!message[key_t].is_raw_string() || message[key_t].as_raw_string().size() != 1))
        throw dht_error(dht_error_protocol, "Invalid transaction ID type/length.");

      // Stupid broken implementations.
      if (nodeId != NULL && *nodeId == m_router->id())
        throw dht_error(dht_error_protocol, "Send your own ID, not mine");

      switch (type) {
        case 'q':
          process_query(*nodeId, sa, message);
          break;

        case 'r':
          process_response(*nodeId, sa, message);
          break;

        case 'e':
          process_error(sa, message);
          break;

        default:
          throw dht_error(dht_error_bad_method, "Unknown message type.");
      }

    // If node was querying us, reply with error packet, otherwise mark the node as "query failed",
    // so that if it repeatedly sends malformed replies we will drop it instead of propagating it
    // to other nodes.
    } catch (const bencode_error& e) {
      if ((type == 'r' || type == 'e') && nodeId != NULL) {
        m_router->node_inactive(*nodeId, sa);
      } else {
        snprintf(message.data_end, message.data + torrent::DhtMessage::data_size - message.data_end - 1, "Malformed packet: %s", e.what());
        message.data[torrent::DhtMessage::data_size - 1] = '\0';
        create_error(message, sa, dht_error_protocol, message.data_end);
      }

    } catch (const dht_error& e) {
      if ((type == 'r' || type == 'e') && nodeId != NULL)
        m_router->node_inactive(*nodeId, sa);
      else
        create_error(message, sa, e.code(), e.what());

    } catch (const network_error&) {
    }
  }

  start_write();
}

void
DhtServer::process_queue(packet_queue& queue) {
  while (!queue.empty()) {
    auto packet = queue.front();
    DhtTransaction::key_type transactionKey = 0;

    if(packet->has_transaction())
      transactionKey = packet->transaction()->key(packet->id());

    // Make sure its transaction hasn't timed out yet, if it has/had one
    // and don't bother sending non-transaction packets (replies) after
    // more than 15 seconds in the queue.
    if (packet->has_failed() || packet->age() > 15) {
      queue.pop_front();
      continue;
    }

    queue.pop_front();

    try {
      int written = write_datagram_sa(packet->c_str(), packet->length(), packet->address());

      if (written == -1)
        throw network_error();

      if (static_cast<unsigned int>(written) != packet->length())
        throw network_error();

    } catch (const network_error&) {
      // Couldn't write packet, maybe something wrong with node address or routing, so mark node as bad.
      if (packet->has_transaction()) {
        auto itr = m_transactions.find(transactionKey);
        if (itr == m_transactions.end())
          throw internal_error("DhtServer::process_queue could not find transaction.");

        failed_transaction(itr, false);
      }
    }

    if (packet->has_transaction()) {
      // here transaction can be already deleted by failed_transaction.
      auto itr = m_transactions.find(transactionKey);

      if (itr != m_transactions.end())
        packet->transaction()->set_packet(NULL);
    }
  }
}

void
DhtServer::event_write() {
  if (m_highQueue.empty() && m_lowQueue.empty())
    throw internal_error("DhtServer::event_write called but both write queues are empty.");

  process_queue(m_highQueue);
  process_queue(m_lowQueue);

  if (m_highQueue.empty() && m_lowQueue.empty())
    this_thread::poll()->remove_write(this);
}

void
DhtServer::event_error() {
}

void
DhtServer::start_write() {
  if (!m_highQueue.empty() || !m_lowQueue.empty())
    this_thread::poll()->insert_write(this);

  if (!m_task_timeout.is_scheduled() && !m_transactions.empty())
    this_thread::scheduler()->wait_for_ceil_seconds(&m_task_timeout, 5s);
}

void
DhtServer::receive_timeout() {
  auto itr = m_transactions.begin();
  while (itr != m_transactions.end()) {
    if (itr->second->has_quick_timeout() && itr->second->quick_timeout() < this_thread::cached_seconds().count()) {
      itr = failed_transaction(itr, true);

    } else if (itr->second->timeout() < this_thread::cached_seconds().count()) {
      itr = failed_transaction(itr, false);

    } else {
      ++itr;
    }
  }

  start_write();
}

} // namespace torrent
