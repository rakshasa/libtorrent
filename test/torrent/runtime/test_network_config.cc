#include "config.h"

#include "test_network_config.h"

#include "runtime_manager.h"
#include "torrent/runtime/network_config.h"
#include "torrent/system/scheduler.h"

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(test_network_config, "torrent/runtime");

void
test_network_config::test_destroy_with_pending_notification() {
  auto scheduler = torrent::this_thread::scheduler();

  // The setter schedules the change notification 200 ms later.
  torrent::runtime::network_config()->set_block_ipv6(true);
  CPPUNIT_ASSERT(scheduler->next_timeout(1h) < 1h);

  torrent::RuntimeManager::destroy();

  // tearDown() needs a runtime.
  torrent::RuntimeManager::initialize();

  CPPUNIT_ASSERT(scheduler->next_timeout(1h) == 1h);
}
