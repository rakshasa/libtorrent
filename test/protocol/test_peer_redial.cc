#include "config.h"

#include "test/protocol/test_peer_redial.h"

#include <algorithm>
#include <string>
#include <vector>

#include <netinet/in.h>
#include <arpa/inet.h>

#include "download/available_list.h"
#include "torrent/download_info.h"
#include "torrent/net/socket_address.h"
#include "torrent/peer/connection_list.h"
#include "torrent/peer/peer_info.h"
#include "torrent/peer/peer_list.h"

CPPUNIT_TEST_SUITE_REGISTRATION(test_peer_redial);

using namespace std::chrono_literals;

namespace {

using torrent::ConnectionList;
using torrent::PeerList;

// PeerList's connect and disconnect calls are for libtorrent's own classes; a test reaches them through a subclass.
class peer_list : public PeerList {
public:
  using PeerList::connected;
  using PeerList::disconnected;
  using PeerList::set_info;
};

sockaddr_in
address(const char* ip, uint16_t port) {
  sockaddr_in sa{};
  sa.sin_family = AF_INET;
  sa.sin_port   = htons(port);
  inet_pton(AF_INET, ip, &sa.sin_addr);
  return sa;
}

sockaddr_in6
address6(const char* ip, uint16_t port) {
  sockaddr_in6 sa{};
  sa.sin6_family = AF_INET6;
  sa.sin6_port   = htons(port);
  inet_pton(AF_INET6, ip, &sa.sin6_addr);
  return sa;
}

template <typename T>
const sockaddr*
sa(const T& s) {
  return reinterpret_cast<const sockaddr*>(&s);
}

// The addresses on the download's list to dial, as "ip:port".
std::vector<std::string>
dial_list(peer_list& list) {
  std::vector<std::string> out;
  for (const auto& a : *list.available_list())
    out.push_back(torrent::sa_addr_str(&a.sa) + ":" + std::to_string(torrent::sa_port(&a.sa)));
  return out;
}

bool
listed(peer_list& list, const std::string& s) {
  auto l = dial_list(list);
  return std::find(l.begin(), l.end(), s) != l.end();
}

constexpr int closed_on_us = ConnectionList::disconnect_available;  // the peer closed, the socket failed, silence
constexpr int redial       = PeerList::disconnect_set_time | PeerList::disconnect_available;

} // namespace

void
test_peer_redial::test_wanted_only_for_closes_we_did_not_choose() {
  CPPUNIT_ASSERT(ConnectionList::redial_wanted(closed_on_us, true, false, 0));

  // p.disconnect, a delayed disconnect, a torrent's stop: nothing asks for the peer back
  CPPUNIT_ASSERT(!ConnectionList::redial_wanted(0, true, false, 16384));
  CPPUNIT_ASSERT(!ConnectionList::redial_wanted(ConnectionList::disconnect_quick, true, false, 16384));

  // bad pieces, a seed while we seed
  CPPUNIT_ASSERT(!ConnectionList::redial_wanted(closed_on_us | ConnectionList::disconnect_unwanted, true, false, 16384));
}

void
test_peer_redial::test_wanted_only_while_downloading() {
  // stopped, complete (seeding) or fetching a magnet's metadata
  CPPUNIT_ASSERT(!ConnectionList::redial_wanted(closed_on_us, false, false, 16384));
  CPPUNIT_ASSERT(!ConnectionList::redial_wanted(closed_on_us | ConnectionList::disconnect_quick, false, false, 16384));
}

void
test_peer_redial::test_wanted_for_dialled_or_sending_peers() {
  CPPUNIT_ASSERT(ConnectionList::redial_wanted(closed_on_us, true, false, 0));     // we dialled it
  CPPUNIT_ASSERT(ConnectionList::redial_wanted(closed_on_us, true, true, 1));      // it dialled us and sent data
  CPPUNIT_ASSERT(!ConnectionList::redial_wanted(closed_on_us, true, true, 0));     // it dialled us and sent nothing
}

void
test_peer_redial::test_queued_on_listen_port() {
  torrent::DownloadInfo info;
  peer_list             list;
  list.set_info(&info);

  auto a    = address("198.18.1.1", 40001);
  auto peer = list.connected(sa(a), 0);  // we dial it
  CPPUNIT_ASSERT(peer != nullptr);
  CPPUNIT_ASSERT(list.available_list()->empty());

  list.disconnected(peer, redial);

  // on the port we dialled, not on port 0 (the connection's port is cleared on disconnect)
  CPPUNIT_ASSERT(dial_list(list) == std::vector<std::string>{"198.18.1.1:40001"});
  CPPUNIT_ASSERT(torrent::sa_port(peer->socket_address()) == 0);
  CPPUNIT_ASSERT(peer->last_redial() == static_cast<uint32_t>(torrent::this_thread::cached_seconds().count()));
}

void
test_peer_redial::test_incoming_queued_on_listen_port() {
  torrent::DownloadInfo info;
  peer_list             list;
  list.set_info(&info);

  // we had dialled it on 40001 once (the port it listens on), later it dialled us from 55000
  auto told  = address("198.18.1.1", 40001);
  auto first = list.connected(sa(told), 0);
  CPPUNIT_ASSERT(first != nullptr);
  list.disconnected(first, PeerList::disconnect_set_time);

  auto from = address("198.18.1.1", 55000);
  auto peer = list.connected(sa(from), PeerList::connect_incoming);
  CPPUNIT_ASSERT(peer != nullptr);
  CPPUNIT_ASSERT(peer->listen_port() == 40001);

  list.disconnected(peer, redial);

  CPPUNIT_ASSERT(dial_list(list) == std::vector<std::string>{"198.18.1.1:40001"});
}

void
test_peer_redial::test_incoming_without_listen_port_not_queued() {
  torrent::DownloadInfo info;
  peer_list             list;
  list.set_info(&info);

  // it dialled us and never said where it listens: nothing to dial
  auto from = address("198.18.1.1", 55000);
  auto peer = list.connected(sa(from), PeerList::connect_incoming);
  CPPUNIT_ASSERT(peer != nullptr);
  CPPUNIT_ASSERT(peer->listen_port() == 0);

  list.disconnected(peer, redial);

  CPPUNIT_ASSERT(list.available_list()->empty());
  CPPUNIT_ASSERT(peer->last_redial() == 0);
}

void
test_peer_redial::test_inet6_queued_on_listen_port() {
  torrent::DownloadInfo info;
  peer_list             list;
  list.set_info(&info);

  auto a    = address6("2001:db8::7", 40001);
  auto peer = list.connected(sa(a), 0);
  CPPUNIT_ASSERT(peer != nullptr);

  list.disconnected(peer, redial);

  CPPUNIT_ASSERT(list.available_list()->size() == 1);
  CPPUNIT_ASSERT(torrent::sa_port(&list.available_list()->begin()->sa) == 40001);
  CPPUNIT_ASSERT(torrent::sa_addr_str(&list.available_list()->begin()->sa) == "2001:db8::7");
}

void
test_peer_redial::test_queued_peer_passes_recent_filter() {
  torrent::DownloadInfo info;
  peer_list             list;
  list.set_info(&info);

  auto a    = address("198.18.1.1", 40001);
  auto peer = list.connected(sa(a), PeerList::connect_filter_recent);
  CPPUNIT_ASSERT(peer != nullptr);

  // a minute later the connection closes: the handshake is a minute old
  m_main_thread->test_add_cached_time(60s);
  list.disconnected(peer, redial);

  // the dial that follows is not refused as a recent handshake (it would be for ten minutes)
  CPPUNIT_ASSERT(list.connected(sa(a), PeerList::connect_filter_recent) == peer);
}

void
test_peer_redial::test_already_listed_peer_passes_recent_filter() {
  torrent::DownloadInfo info;
  peer_list             list;
  list.set_info(&info);

  // we dialled it on 40001 once (the port it listens on)
  auto told = address("198.18.1.1", 40001);
  auto peer = list.connected(sa(told), 0);
  list.disconnected(peer, PeerList::disconnect_set_time);

  // later a tracker's answer puts 40001 on the list, and before we dial it, it dials us from 55000
  m_main_thread->test_add_cached_time(700s);
  list.available_list()->insert_unique(sa(told));
  auto from = address("198.18.1.1", 55000);
  CPPUNIT_ASSERT(list.connected(sa(from), PeerList::connect_incoming) == peer);

  // a minute later it closes: the address is on the list already, and the dial must still pass the filter
  m_main_thread->test_add_cached_time(60s);
  list.disconnected(peer, redial);

  CPPUNIT_ASSERT(dial_list(list) == std::vector<std::string>{"198.18.1.1:40001"});
  CPPUNIT_ASSERT(peer->last_redial() != 0);
  CPPUNIT_ASSERT(list.connected(sa(told), PeerList::connect_filter_recent) == peer);
}

void
test_peer_redial::test_once_per_interval() {
  torrent::DownloadInfo info;
  peer_list             list;
  list.set_info(&info);

  auto a    = address("198.18.1.1", 40001);
  auto peer = list.connected(sa(a), 0);
  list.disconnected(peer, redial);
  CPPUNIT_ASSERT(list.available_list()->size() == 1);

  list.available_list()->pop_random();  // dialled
  CPPUNIT_ASSERT(list.connected(sa(a), PeerList::connect_filter_recent) == peer);

  // it closes again within the interval: not back on the list
  m_main_thread->test_add_cached_time(std::chrono::seconds(PeerList::redial_interval - 1));
  list.disconnected(peer, redial);
  CPPUNIT_ASSERT(list.available_list()->empty());

  // after the interval it is
  CPPUNIT_ASSERT(list.connected(sa(a), 0) == peer);
  m_main_thread->test_add_cached_time(1s);
  list.disconnected(peer, redial);
  CPPUNIT_ASSERT(listed(list, "198.18.1.1:40001"));
}

void
test_peer_redial::test_not_for_bad_or_banned_peers() {
  torrent::DownloadInfo info;
  peer_list             list;
  list.set_info(&info);

  auto bad  = address("198.18.1.1", 40001);
  auto peer = list.connected(sa(bad), 0);
  peer->set_failed_counter(1);  // one bad piece
  list.disconnected(peer, redial);

  auto banned = address("198.18.1.2", 40001);
  peer = list.connected(sa(banned), 0);
  peer->set_failed_counter(64);  // p.banned.set
  list.disconnected(peer, redial);

  CPPUNIT_ASSERT(list.available_list()->empty());
}

void
test_peer_redial::test_not_past_list_limit() {
  torrent::DownloadInfo info;
  peer_list             list;
  list.set_info(&info);

  list.available_list()->set_max_size(1);

  auto a = address("198.18.1.1", 40001);
  auto b = address("198.18.1.2", 40001);
  auto pa = list.connected(sa(a), 0);
  auto pb = list.connected(sa(b), 0);

  list.disconnected(pa, redial);
  list.disconnected(pb, redial);

  CPPUNIT_ASSERT(dial_list(list) == std::vector<std::string>{"198.18.1.1:40001"});
  CPPUNIT_ASSERT(pb->last_redial() == 0);
}

void
test_peer_redial::test_plain_disconnect_not_queued() {
  torrent::DownloadInfo info;
  peer_list             list;
  list.set_info(&info);

  auto a    = address("198.18.1.1", 40001);
  auto peer = list.connected(sa(a), 0);
  list.disconnected(peer, PeerList::disconnect_set_time);

  CPPUNIT_ASSERT(list.available_list()->empty());
  CPPUNIT_ASSERT(peer->last_redial() == 0);
}
