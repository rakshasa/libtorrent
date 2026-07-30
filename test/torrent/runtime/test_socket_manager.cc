#include "config.h"

#include "test_socket_manager.h"

#include <array>

#include "runtime_manager.h"
#include "torrent/exceptions.h"
#include "torrent/runtime/socket_manager.h"

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(test_socket_manager, "torrent/runtime");

namespace {

using category_array = std::array<uint32_t, torrent::runtime::SocketManager::category_count>;

category_array
current_max_sizes() {
  category_array result{};

  for (uint32_t i = 0; i < result.size(); i++)
    result[i] = torrent::runtime::socket_manager()->category_max_size(static_cast<torrent::runtime::socket_manager_category_t>(i));

  return result;
}

} // namespace

void
test_socket_manager::setUp() {
  test_fixture::setUp();

  torrent::RuntimeManager::initialize();
}

void
test_socket_manager::tearDown() {
  torrent::RuntimeManager::destroy();

  test_fixture::tearDown();
}

void
test_socket_manager::test_basic() {
  auto socket_manager = torrent::runtime::socket_manager();

  socket_manager->set_max_size_and_adjust(1024);

  CPPUNIT_ASSERT_EQUAL(uint32_t{1024}, socket_manager->max_size());
  CPPUNIT_ASSERT_EQUAL(uint32_t{32},   socket_manager->category_max_size(torrent::runtime::category_internal));
  CPPUNIT_ASSERT_EQUAL(uint32_t{32},   socket_manager->category_max_size(torrent::runtime::category_http));
  CPPUNIT_ASSERT_EQUAL(uint32_t{32},   socket_manager->category_max_size(torrent::runtime::category_rpc));
  CPPUNIT_ASSERT_EQUAL(uint32_t{128},  socket_manager->category_max_size(torrent::runtime::category_files));
  CPPUNIT_ASSERT_EQUAL(uint32_t{672},  socket_manager->category_max_size(torrent::runtime::category_generic));

  CPPUNIT_ASSERT_THROW(socket_manager->set_max_size_and_adjust(511), torrent::input_error);
}

void
test_socket_manager::test_adjust_allocation_over_budget() {
  auto socket_manager = torrent::runtime::socket_manager();

  socket_manager->set_max_size_and_adjust(1024);

  auto expected_max_sizes = current_max_sizes();

  socket_manager->set_category_min_allocation(torrent::runtime::category_files, 1000000);

  CPPUNIT_ASSERT_THROW(socket_manager->adjust_allocation(), torrent::input_error);

  CPPUNIT_ASSERT(current_max_sizes() == expected_max_sizes);
  CPPUNIT_ASSERT_EQUAL(uint32_t{1024}, socket_manager->max_size());

  CPPUNIT_ASSERT_THROW(socket_manager->set_max_size_and_adjust(2048), torrent::input_error);

  CPPUNIT_ASSERT(current_max_sizes() == expected_max_sizes);
  CPPUNIT_ASSERT_EQUAL(uint32_t{1024}, socket_manager->max_size());
}

void
test_socket_manager::test_adjust_allocation_staged_min_alloc() {
  auto socket_manager = torrent::runtime::socket_manager();

  socket_manager->set_max_size_and_adjust(8096);

  CPPUNIT_ASSERT_EQUAL(uint32_t{64},   socket_manager->category_max_size(torrent::runtime::category_http));
  CPPUNIT_ASSERT_EQUAL(uint32_t{48},   socket_manager->category_max_size(torrent::runtime::category_rpc));
  CPPUNIT_ASSERT_EQUAL(uint32_t{6672}, socket_manager->category_max_size(torrent::runtime::category_generic));

  socket_manager->set_category_min_allocation(torrent::runtime::category_http, 400);
  socket_manager->set_category_min_allocation(torrent::runtime::category_rpc, 64);
  socket_manager->adjust_allocation();

  CPPUNIT_ASSERT_EQUAL(uint32_t{400},  socket_manager->category_max_size(torrent::runtime::category_http));
  CPPUNIT_ASSERT_EQUAL(uint32_t{64},   socket_manager->category_max_size(torrent::runtime::category_rpc));
  CPPUNIT_ASSERT_EQUAL(uint32_t{6320}, socket_manager->category_max_size(torrent::runtime::category_generic));

  socket_manager->set_category_min_allocation(torrent::runtime::category_rpc, 400);

  CPPUNIT_ASSERT_THROW(socket_manager->adjust_allocation(), torrent::input_error);

  CPPUNIT_ASSERT_EQUAL(uint32_t{400},  socket_manager->category_max_size(torrent::runtime::category_http));
  CPPUNIT_ASSERT_EQUAL(uint32_t{64},   socket_manager->category_max_size(torrent::runtime::category_rpc));
  CPPUNIT_ASSERT_EQUAL(uint32_t{6320}, socket_manager->category_max_size(torrent::runtime::category_generic));

  socket_manager->set_category_min_allocation(torrent::runtime::category_http, 64);
  socket_manager->adjust_allocation();

  CPPUNIT_ASSERT_EQUAL(uint32_t{64},   socket_manager->category_max_size(torrent::runtime::category_http));
  CPPUNIT_ASSERT_EQUAL(uint32_t{400},  socket_manager->category_max_size(torrent::runtime::category_rpc));
  CPPUNIT_ASSERT_EQUAL(uint32_t{6320}, socket_manager->category_max_size(torrent::runtime::category_generic));
}
