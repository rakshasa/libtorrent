#include "config.h"

#include "webseed_controller_test.h"

#include "torrent/webseed_controller.h"

CPPUNIT_TEST_SUITE_REGISTRATION(WebseedControllerTest);

void
WebseedControllerTest::setUp() {
}

void
WebseedControllerTest::tearDown() {
}


namespace torrent{
  class Manager {};
 
  Manager manager;
}

void
WebseedControllerTest::test_basic() {
  torrent::WebseedController controller(NULL);

  CPPUNIT_ASSERT(controller.url_list()->size() == 0);
  controller.add_url("http://test/test");
  CPPUNIT_ASSERT(controller.url_list()->size() == 1);

  char* buffer = (char*)malloc(1000);
  controller.download_to_buffer(buffer, "file://blah", 0, 1000);
  free(buffer);
}
