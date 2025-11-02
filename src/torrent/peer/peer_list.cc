#include "config.h"

#include "peer_list.h"

#include <algorithm>

#include "manager.h"
#include "download/available_list.h"
#include "torrent/download_info.h"
#include "torrent/exceptions.h"
#include "torrent/net/socket_address.h"
#include "torrent/peer/client_list.h"
#include "torrent/peer/peer_info.h"
#include "torrent/utils/log.h"

#define LT_LOG_EVENTS(log_fmt, ...)                                     \
  lt_log_print_info(LOG_PEER_LIST_EVENTS, m_info, "peer_list", log_fmt, __VA_ARGS__);
#define LT_LOG_ADDRESS(log_fmt, ...)                                    \
  lt_log_print_info(LOG_PEER_LIST_ADDRESS, m_info, "peer_list", log_fmt, __VA_ARGS__);

namespace torrent {

ipv4_table PeerList::m_ipv4_table;

PeerList::PeerList()
  : m_available_list(new AvailableList) {
}

PeerList::~PeerList() {
  LT_LOG_EVENTS("deleting list total:%zu available:%zu", size(), m_available_list->size());

  base_type::clear();

  m_info = nullptr;
}

void
PeerList::set_info(DownloadInfo* info) {
  m_info = info;

  LT_LOG_EVENTS("creating list", 0);
}

PeerInfo*
PeerList::insert_address(const sockaddr* sa, int flags) {
  socket_address_key sock_key = socket_address_key::from_sockaddr(sa);

  if (sock_key.is_valid() && !socket_address_key::is_comparable_sockaddr(sa)) {
    LT_LOG_EVENTS("adding address: not comparable: %s", sa_pretty_str(sa).c_str());
    return nullptr;
  }

  range_type range = base_type::equal_range(sock_key);

  auto addr_str = sa_addr_str(sa);
  auto port = sa_port(sa);

  // Do some special handling if we got a new port number but the
  // address was present.
  //
  // What we do depends on the flags, but for now just allow one
  // PeerInfo per address key and do nothing.
  if (range.first != range.second) {
    LT_LOG_EVENTS("adding address: already exists: %s", sa_pretty_str(sa).c_str());
    return nullptr;
  }

  auto peer_info = std::make_unique<PeerInfo>(sa);
  peer_info->set_listen_port(port);

  if (sa->sa_family == AF_INET) {
    // IPv4 addresses are stored in host byte order in ipv4_table so they are comparable.
    uint32_t host_byte_order_ipv4_addr = ntohl(reinterpret_cast<const sockaddr_in*>(sa)->sin_addr.s_addr);

    if(m_ipv4_table.defined(host_byte_order_ipv4_addr))
      peer_info->set_flags(m_ipv4_table.at(host_byte_order_ipv4_addr) & PeerInfo::mask_ip_table);

  } else if (sa->sa_family == AF_INET6) {
    // Currently nothing to do for IPv6 addresses.

  } else {
    throw internal_error("PeerList::insert_address() only supports INET/INET6 addresses");
  }

  manager->client_list()->retrieve_unknown(&peer_info->mutable_client_info());

  if ((flags & address_available) && peer_info->listen_port() != 0) {
    m_available_list->insert_unique(sa);

    LT_LOG_EVENTS("adding address: available : %s", sa_pretty_str(sa).c_str());
  } else {
    LT_LOG_EVENTS("adding address: unavailable : %s", sa_pretty_str(sa).c_str());
  }

  auto itr = base_type::insert(range.second, value_type(sock_key, peer_info.release()));

  return itr->second.get();
}

uint32_t
PeerList::insert_available(const void* al) {
  auto address_list = static_cast<const AddressList*>(al);

  uint32_t inserted = 0;
  uint32_t invalid = 0;
  uint32_t unneeded = 0;
  uint32_t updated = 0;

  if (m_available_list->size() + address_list->size() > m_available_list->capacity())
    m_available_list->reserve(m_available_list->size() + address_list->size() + 128);

  // Optimize this so that we don't traverse the tree for every
  // insert, since we know 'al' is sorted.

  auto avail_itr  = m_available_list->begin();
  auto avail_last = m_available_list->end();

  for (const auto& addr : *address_list) {
    auto addr_str = sa_addr_str(&addr.sa);
    auto port = sa_port(&addr.sa);

    if (!socket_address_key::is_comparable_sockaddr(&addr.sa) || port == 0) {
      invalid++;
      LT_LOG_ADDRESS("adding available address: skipped invalid : %s", sa_pretty_str(&addr.sa).c_str());
      continue;
    }

    // TODO: Verify we only want to check the address and not the port.

    avail_itr = std::find_if(avail_itr, avail_last, [&addr](auto& sa) {
        return sa_less_addr(&sa.sa, &addr.sa);
      });

    if (avail_itr != avail_last && !sa_less_addr(&avail_itr->sa, &addr.sa)) {
      // The address is already in m_available_list, so don't bother
      // going further.
      unneeded++;
      continue;
    }

    socket_address_key sock_key = socket_address_key::from_sockaddr(&addr.sa);

    // Check if the peerinfo exists, if it does, check if we would
    // ever want to connect. Just update the timer for the last
    // availability notice if the peer isn't really ideal, but might
    // be used in an emergency.
    range_type range = base_type::equal_range(sock_key);

    if (range.first != range.second) {
      // Add some logic here to select the best PeerInfo, but for now
      // just assume the first one is the only one that exists.
      PeerInfo* peer_info = range.first->second.get();

      if (peer_info->listen_port() == 0)
        peer_info->set_port(port);

      if (peer_info->connection() != nullptr ||
          peer_info->last_handshake() + 600 > static_cast<uint32_t>(this_thread::cached_seconds().count())) {
        updated++;
        continue;
      }

      // If the peer has sent us bad chunks or we just connected or
      // tried to do so a few minutes ago, only update its
      // availability timer.
    }

    // Should we perhaps add to available list even though we don't
    // want the peer, just to ensure we don't need to search for the
    // PeerInfo every time it gets reported. Though I'd assume it
    // won't happen often enough to be worth it.

    inserted++;
    m_available_list->insert_unique(&addr.sa);

    LT_LOG_ADDRESS("adding available address: %s", sa_pretty_str(&addr.sa).c_str());
  }

  LT_LOG_EVENTS("inserted peers"
                " inserted:%" PRIu32 " invalid:%" PRIu32
                " unneeded:%" PRIu32 " updated:%" PRIu32
                " total:%" PRIuPTR " available:%" PRIuPTR,
                inserted, invalid, unneeded, updated,
                size(), m_available_list->size());

  return inserted;
}

uint32_t
PeerList::available_list_size() const {
  return m_available_list->size();
}

// TODO: Rename connecting:
PeerInfo*
PeerList::connected(const sockaddr* sa, int flags) {
  // TODO: Rewrite to use new socket address api after fixing bug.

  // TODO: Handle 4in6 addresses.

  auto sock_key = socket_address_key::from_sockaddr(sa);
  auto addr_str = sa_addr_str(sa);
  auto port = sa_port(sa);

  if (!sock_key.is_valid() || !socket_address_key::is_comparable_sockaddr(sa))
    return nullptr;

  int filter_value = 0;

  if (sa->sa_family == AF_INET) {
    uint32_t host_byte_order_ipv4_addr = ntohl(reinterpret_cast<const sockaddr_in*>(sa)->sin_addr.s_addr);

    // IPv4 addresses stored in host byte order in ipv4_table so they are comparable. ntohl has been called
    if(m_ipv4_table.defined(host_byte_order_ipv4_addr))
      filter_value = m_ipv4_table.at(host_byte_order_ipv4_addr);

    // We should also remove any PeerInfo objects already for this
    // address.
    if ((filter_value & PeerInfo::flag_unwanted)) {
      LT_LOG_EVENTS("connecting peer rejected: flagged as unwanted : %s", sa_pretty_str(sa).c_str());
      return nullptr;
    }

  } else if (sa->sa_family == AF_INET6) {
    // Filtering not supported for IPv6 yet.

  } else {
    throw internal_error("PeerList::connected() with invalid address family");
  }

  PeerInfo* peer_info;
  range_type range = base_type::equal_range(sock_key);

  if (range.first == range.second) {
    // Create a new entry.
    peer_info = new PeerInfo(sa);
    peer_info->set_flags(filter_value & PeerInfo::mask_ip_table);

    base_type::insert(range.second, value_type(sock_key, peer_info));

  } else if (!range.first->second->is_connected()) {
    // Use an old entry.
    peer_info = range.first->second.get();
    peer_info->set_port(port);

  } else {
    // Make sure we don't end up throwing away the port the host is
    // actually listening on, when there may be several simultaneous
    // connection attempts to/from different ports.
    //
    // This also ensure we can connect to peers running on the same
    // host as the tracker.
    // if (flags & connect_keep_handshakes &&
    //     range.first->second->is_handshake() &&
    //     range.first->second->socket_address()->port() != address->port())
    //   m_available_list->buffer()->push_back(*address);

    LT_LOG_EVENTS("connecting peer rejected: already connected (buggy, fixme) : %s", sa_pretty_str(sa).c_str());

    // TODO: Verify this works properly, possibly add a check/flag
    // that allows the handshake manager to notify peer list if the
    // incoming connection was a duplicate peer hash.

    //return nullptr;

    peer_info = new PeerInfo(sa);
    peer_info->set_flags(filter_value & PeerInfo::mask_ip_table);

    base_type::insert(range.second, value_type(sock_key, peer_info));
  }

  if ((flags & connect_filter_recent) &&
      peer_info->last_handshake() + 600 > static_cast<uint32_t>(this_thread::cached_seconds().count())) {
    LT_LOG_EVENTS("connecting peer rejected: recent handshake : %s", sa_pretty_str(sa).c_str());
    return nullptr;
  }

  peer_info->set_flags(PeerInfo::flag_connected);
  peer_info->set_last_handshake(this_thread::cached_seconds().count());

  if ((flags & connect_incoming)) {
    peer_info->set_flags(PeerInfo::flag_incoming);

    LT_LOG_EVENTS("connecting peer accepted: incoming : %s", sa_pretty_str(sa).c_str());

  } else {
    peer_info->unset_flags(PeerInfo::flag_incoming);
    peer_info->set_listen_port(port);

    LT_LOG_EVENTS("connecting peer accepted: outgoing : %s", sa_pretty_str(sa).c_str());
  }

  return peer_info;
}

// Make sure we properly clear port when disconnecting.

void
PeerList::disconnected(PeerInfo* p, int flags) {
  socket_address_key sock_key = socket_address_key::from_sockaddr(p->socket_address());

  range_type range = base_type::equal_range(sock_key);

  auto itr = std::find_if(range.first, range.second, [p](auto& v) { return p == v.second.get(); });

  if (itr == range.second) {
    if (std::none_of(base_type::begin(), base_type::end(), [p](auto& v){ return p == v.second.get(); }))
      throw internal_error("PeerList::disconnected(...) itr == range.second, doesn't exist.");
    else
      throw internal_error("PeerList::disconnected(...) itr == range.second, not in the range.");
  }

  disconnected(itr, flags);
}

PeerList::iterator
PeerList::disconnected(iterator itr, int flags) {
  if (itr == base_type::end())
    throw internal_error("PeerList::disconnected(...) itr == end().");

  if (!itr->second->is_connected())
    throw internal_error("PeerList::disconnected(...) !itr->is_connected().");

  if (itr->second->transfer_counter() != 0) {
    // Currently we only log these as it only affects the culling of
    // peers.
    LT_LOG_EVENTS("disconnected with non-zero transfer counter (%" PRIu32 ") for peer %40s",
                  itr->second->transfer_counter(), itr->second->id_hex());
  }

  itr->second->unset_flags(PeerInfo::flag_connected);

  // Replace the socket address port with the listening port so that
  // future outgoing connections will connect to the right port.
  itr->second->set_port(0);

  if (flags & disconnect_set_time)
    itr->second->set_last_connection(this_thread::cached_seconds().count());

  if (flags & disconnect_available && itr->second->listen_port() != 0)
    m_available_list->insert_unique(itr->second->socket_address());

  // Do magic to get rid of unneeded entries.
  return ++itr;
}

uint32_t
PeerList::cull_peers(int flags) {
  uint32_t counter = 0;
  uint32_t timer;

  if (flags & cull_old)
    timer = this_thread::cached_seconds().count() - 24 * 60 * 60;
  else
    timer = 0;

  for (auto itr = base_type::begin(); itr != base_type::end(); ) {
    if (itr->second->is_connected() ||
        itr->second->transfer_counter() != 0 || // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
        itr->second->last_connection() >= timer ||

        (flags & cull_keep_interesting &&
         (itr->second->failed_counter() != 0 || itr->second->is_blocked()))) {
      itr++;
      continue;
    }

    // ##################### TODO: LOG CULLING OF PEERS ######################
    //   *** AND STATS OF DISCONNECTING PEERS (the peer info...)...

    // The key is a pointer to a member in the value, although the key
    // shouldn't actually be used in erase (I think), just ot be safe
    // we delete it after erase.
    auto tmp = itr++;
    base_type::erase(tmp);

    counter++;
  }

  return counter;
}

uint32_t
PeerList::insert_pex_list(const raw_string& pex_list) {
  if (pex_list.empty())
    return true;

  AddressList l;

  l.parse_address_compact(pex_list);
  l.sort_and_unique();

  LT_LOG_EVENTS("inserting pex list: %" PRIu32 " peers", l.size());

  return insert_available(&l);
}

} // namespace torrent
