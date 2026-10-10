#include "config.h"

#include "test/protocol/test_pex_unroutable.h"

#include <arpa/inet.h>
#include <cstring>
#include <memory>

#include "download/download_main.h"
#include "protocol/extensions.h"
#include "torrent/peer/peer_list.h"

CPPUNIT_TEST_SUITE_REGISTRATION(TestPexUnroutable);

namespace {

std::string
compact6(const char* text, const char* port) {
  in6_addr addr{};

  if (inet_pton(AF_INET6, text, &addr) != 1)
    throw std::runtime_error("bad test address");

  return std::string(reinterpret_cast<const char*>(&addr), 16) + std::string(port, 2);
}

} // namespace

// rtorrent16: a ut_pex message through ProtocolExtension, as a peer sends it. Upstream 6f1626d7 skips unroutable
// addresses in PeerList::insert_available(), the door tracker, DHT and PEX peers all go through; the "added6" list
// that patches/libtorrent-dht-ipv6.patch reads must still bring in a routable IPv6 peer, and must not bring in
// loopback, link-local (unicast or multicast), the unspecified address or a v4-mapped loopback / link-local one.
void
TestPexUnroutable::test_ut_pex_skips_unroutable_added6() {
  auto download = std::make_unique<torrent::DownloadMain>();

  auto* extensions = new torrent::ProtocolExtension;
  extensions->set_info(nullptr, download.get());
  extensions->set_local_enabled(torrent::ProtocolExtension::UT_PEX);

  std::string added6 = compact6("2a01:e34:ec02:e860:9c66:1f03:bdcb:6428", "\x8b\x92");

  for (auto text : { "::1", "fe80::1", "ff02::1", "::", "::ffff:127.0.0.1", "::ffff:169.254.1.1" })
    added6 += compact6(text, "\x1a\xe1");

  std::string message = "d5:added6:" + std::string("\x01\x02\x03\x04\x1a\xe1", 6) +
                        "6:added6" + std::to_string(added6.size()) + ":" + added6 + "7:dropped0:e";

  extensions->read_start(torrent::ProtocolExtension::UT_PEX, message.size(), false);
  std::memcpy(extensions->read_position(), message.data(), message.size());
  CPPUNIT_ASSERT(extensions->read_move(message.size()));
  CPPUNIT_ASSERT(extensions->read_done());

  // 1.2.3.4 from "added" and the one global IPv6 address from "added6".
  CPPUNIT_ASSERT_EQUAL(uint32_t{2}, download->peer_list()->available_list_size());

  extensions->cleanup();
  delete extensions;
}
