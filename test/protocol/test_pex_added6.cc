#include "config.h"

#include "test/protocol/test_pex_added6.h"

#include <arpa/inet.h>
#include <cstring>
#include <memory>
#include <string>

#include "download/available_list.h"
#include "download/download_main.h"
#include "protocol/extensions.h"
#include "torrent/peer/peer_list.h"

CPPUNIT_TEST_SUITE_REGISTRATION(TestPexAdded6);

namespace {

std::string
compact6(const char* address, uint16_t port) {
  in6_addr addr;
  CPPUNIT_ASSERT(inet_pton(AF_INET6, address, &addr) == 1);

  uint16_t net_port = htons(port);

  std::string result(reinterpret_cast<const char*>(&addr), sizeof(addr));
  result.append(reinterpret_cast<const char*>(&net_port), sizeof(net_port));
  return result;
}

void
receive_pex(torrent::DownloadMain& download, const std::string& added6) {
  std::string message = "d6:added6" + std::to_string(added6.size()) + ":" + added6 + "e";

  torrent::ProtocolExtension extensions;
  extensions.set_info(nullptr, &download);
  extensions.set_local_enabled(torrent::ProtocolExtension::UT_PEX);

  extensions.read_start(torrent::ProtocolExtension::UT_PEX, message.size(), false);
  std::memcpy(extensions.read_position(), message.data(), message.size());
  extensions.read_move(message.size());

  CPPUNIT_ASSERT(extensions.read_done());
}

} // namespace

void
TestPexAdded6::test_valid_peers() {
  auto download = std::make_unique<torrent::DownloadMain>();

  receive_pex(*download, compact6("2001:db8::1", 6881) + compact6("2001:db8::2", 51413));

  CPPUNIT_ASSERT_EQUAL(uint32_t{2}, download->peer_list()->available_list_size());

  auto& available = *download->peer_list()->available_list();
  bool found_6881 = false;
  bool found_51413 = false;

  while (available.size() != 0) {
    auto sa = available.pop_random();

    CPPUNIT_ASSERT_EQUAL(int{AF_INET6}, int{sa.sa.sa_family});
    found_6881 |= ntohs(sa.inet6.sin6_port) == 6881;
    found_51413 |= ntohs(sa.inet6.sin6_port) == 51413;
  }

  CPPUNIT_ASSERT(found_6881 && found_51413);
}

void
TestPexAdded6::test_unroutable_and_invalid_filtered() {
  auto download = std::make_unique<torrent::DownloadMain>();

  receive_pex(*download, compact6("::", 6881) + compact6("::1", 6881) + compact6("fe80::1", 6881) +
              compact6("2001:db8::1", 0) + compact6("2001:db8::3", 6881));

  CPPUNIT_ASSERT_EQUAL(uint32_t{1}, download->peer_list()->available_list_size());
}

void
TestPexAdded6::test_truncated_entry_ignored() {
  auto download = std::make_unique<torrent::DownloadMain>();

  receive_pex(*download, compact6("2001:db8::1", 6881) + std::string("abc"));
  CPPUNIT_ASSERT_EQUAL(uint32_t{1}, download->peer_list()->available_list_size());

  receive_pex(*download, std::string());
  CPPUNIT_ASSERT_EQUAL(uint32_t{1}, download->peer_list()->available_list_size());
}
