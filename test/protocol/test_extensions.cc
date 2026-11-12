#include "config.h"

#include "test/protocol/test_extensions.h"

#include <memory>

#include "download/download_main.h"
#include "protocol/extensions.h"
#include "protocol/peer_connection_state.h"
#include "torrent/download_info.h"

CPPUNIT_TEST_SUITE_REGISTRATION(TestExtensions);

void
TestExtensions::test_pex_count_released_on_destroy() {
  auto download = std::make_unique<torrent::DownloadMain>();

  auto* extensions = new torrent::ProtocolExtension;
  extensions->set_info(nullptr, download.get());

  CPPUNIT_ASSERT_EQUAL(uint32_t{0}, download->info()->size_pex());

  extensions->set_local_enabled(torrent::ProtocolExtension::UT_PEX);
  CPPUNIT_ASSERT_EQUAL(uint32_t{1}, download->info()->size_pex());

  // Torn down without cleanup(), as on the connection-setup failure
  // paths. The PEX count on the download must still be released.
  delete extensions;

  CPPUNIT_ASSERT_EQUAL(uint32_t{0}, download->info()->size_pex());
}

void
TestExtensions::test_pex_count_not_released_twice() {
  auto download = std::make_unique<torrent::DownloadMain>();

  auto* extensions = new torrent::ProtocolExtension;
  extensions->set_info(nullptr, download.get());

  extensions->set_local_enabled(torrent::ProtocolExtension::UT_PEX);
  extensions->cleanup();
  CPPUNIT_ASSERT_EQUAL(uint32_t{0}, download->info()->size_pex());

  // Destruction after a proper cleanup must not decrement again.
  delete extensions;

  CPPUNIT_ASSERT_EQUAL(uint32_t{0}, download->info()->size_pex());
}

void
TestExtensions::test_pending_pex_transitions() {
  torrent::protocol::StateUp up;

  up.schedule_pex_exchange();
  up.schedule_pex_enable();
  up.schedule_pex_disable();
  CPPUNIT_ASSERT(up.has_pending_pex_toggle());
  CPPUNIT_ASSERT(!up.has_pending_pex_enable());
  CPPUNIT_ASSERT(up.has_pending_pex_exchange());

  up.schedule_pex_enable();
  CPPUNIT_ASSERT(up.has_pending_pex_enable());
  CPPUNIT_ASSERT(up.has_pending_pex_exchange());

  up.clear_pending_pex_toggle();
  CPPUNIT_ASSERT(!up.has_pending_pex_toggle());
  CPPUNIT_ASSERT(up.has_pending_pex());

  up.schedule_pex_disable();
  up.clear_pending_pex_exchange();
  CPPUNIT_ASSERT(up.has_pending_pex_toggle());
  CPPUNIT_ASSERT(!up.has_pending_pex_exchange());

  up.clear_pending_pex();
  CPPUNIT_ASSERT(!up.has_pending_pex());
}
