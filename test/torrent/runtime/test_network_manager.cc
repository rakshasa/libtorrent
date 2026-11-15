#include "config.h"

#include "test_network_manager.h"

#include <cerrno>
#include <fcntl.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#include "torrent/exceptions.h"
#include "torrent/net/fd.h"
#include "torrent/net/socket_address.h"
#include "torrent/runtime/client_config.h"
#include "torrent/runtime/network_config.h"
#include "torrent/runtime/network_manager.h"
#include "torrent/runtime/proxy_manager.h"
#include "torrent/runtime/runtime.h"
#include "torrent/torrent.h"

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(test_network_manager, "torrent/runtime");

namespace {

struct bound_ports {
  uint16_t inet{};
  uint16_t inet6{};
  int      inet_fd{-1};
};

void
redirect_listen() {
  mock_redirect(torrent::fd__listen, std::function<int(int, int)>([](int fd, int backlog) { return ::listen(fd, backlog); }));
}

void
redirect_listen_failure(int family) {
  mock_redirect(torrent::fd__listen, std::function<int(int, int)>([family](int fd, int backlog) {
      sockaddr_storage address{};
      socklen_t        length = sizeof(address);

      if (getsockname(fd, reinterpret_cast<sockaddr*>(&address), &length) == 0 && address.ss_family == family) {
        errno = EMFILE;
        return -1;
      }

      return ::listen(fd, backlog);
    }));
}

void
redirect_bind(int blocked_family, uint16_t blocked_port, bound_ports& bound) {
  mock_redirect(torrent::fd__bind, std::function<int(int, const sockaddr*, socklen_t)>([&bound, blocked_family, blocked_port](int fd, const sockaddr* address, socklen_t length) {
      if (address->sa_family == blocked_family && torrent::sa_port(address) == blocked_port) {
        errno = EADDRINUSE;
        return -1;
      }

      auto copy = torrent::sa_copy(address);
      torrent::sa_set_port(copy.get(), 0);

      if (::bind(fd, copy.get(), length) != 0)
        return -1;

      sockaddr_storage actual{};
      socklen_t        actual_length = sizeof(actual);

      if (getsockname(fd, reinterpret_cast<sockaddr*>(&actual), &actual_length) != 0)
        return -1;

      if (address->sa_family == AF_INET) {
        bound.inet = torrent::sa_port(reinterpret_cast<sockaddr*>(&actual));
        bound.inet_fd = fd;
      } else {
        bound.inet6 = torrent::sa_port(reinterpret_cast<sockaddr*>(&actual));
      }

      return 0;
    }));
}

bool
can_connect(int family, uint16_t port) {
  auto address = family == AF_INET ? torrent::sa_make_inet_h(INADDR_LOOPBACK, port) : torrent::sa_make_inet6_any();

  if (family == AF_INET6) {
    reinterpret_cast<sockaddr_in6*>(address.get())->sin6_addr = in6addr_loopback;
    torrent::sa_set_port(address.get(), port);
  }

  int fd = socket(family, SOCK_STREAM, 0);
  CPPUNIT_ASSERT(fd != -1);

  bool connected = connect(fd, address.get(), torrent::sa_length(address.get())) == 0;
  CPPUNIT_ASSERT(close(fd) == 0);
  return connected;
}

} // namespace

void
test_network_manager::setUp() {
  test_fixture::setUp();
  mock_redirect_defaults();

  rlimit limit{};
  if (getrlimit(RLIMIT_NOFILE, &limit) == 0 && limit.rlim_cur < 1024 && limit.rlim_max >= 1024) {
    limit.rlim_cur = 1024;
    setrlimit(RLIMIT_NOFILE, &limit);
  }

  torrent::initialize_main_thread();
  torrent::initialize();
}

void
test_network_manager::tearDown() {
  torrent::cleanup();

  test_fixture::tearDown();
}

void
test_network_manager::test_listen_port_change() {
  redirect_listen();
  auto manager = torrent::runtime::network_manager();
  torrent::runtime::initialize_network();
  uint16_t old = manager->listen_port();
  auto range = torrent::runtime::client_config()->listen_port_range();
  uint16_t blocked = old == 1 ? 2 : 1;
  uint16_t next = 1;
  while (next == old || next == blocked)
    ++next;
  bound_ports bound;
  redirect_bind(AF_INET6, blocked, bound);

  CPPUNIT_ASSERT_THROW(manager->set_listen_port(blocked), torrent::input_error);
  CPPUNIT_ASSERT_EQUAL(old, manager->listen_port());
  CPPUNIT_ASSERT(range == torrent::runtime::client_config()->listen_port_range());
  CPPUNIT_ASSERT(manager->is_listening());
  CPPUNIT_ASSERT(bound.inet_fd != -1);
  CPPUNIT_ASSERT(fcntl(bound.inet_fd, F_GETFD) == -1 && errno == EBADF);
  CPPUNIT_ASSERT(can_connect(AF_INET, old));
  CPPUNIT_ASSERT(can_connect(AF_INET6, old));

  redirect_listen_failure(AF_INET);
  CPPUNIT_ASSERT_THROW(manager->set_listen_port(next), torrent::input_error);
  CPPUNIT_ASSERT_EQUAL(old, manager->listen_port());
  CPPUNIT_ASSERT(range == torrent::runtime::client_config()->listen_port_range());
  CPPUNIT_ASSERT(can_connect(AF_INET, old));
  CPPUNIT_ASSERT(can_connect(AF_INET6, old));

  redirect_listen_failure(AF_INET6);
  CPPUNIT_ASSERT_THROW(manager->set_listen_port(next), torrent::input_error);
  CPPUNIT_ASSERT_EQUAL(old, manager->listen_port());
  CPPUNIT_ASSERT(can_connect(AF_INET, old));
  CPPUNIT_ASSERT(can_connect(AF_INET6, old));

  redirect_listen();
  manager->set_listen_port(next);
  CPPUNIT_ASSERT_EQUAL(next, manager->listen_port());
  CPPUNIT_ASSERT(torrent::runtime::client_config()->listen_port_range() == std::make_pair(next, next));
  CPPUNIT_ASSERT(manager->is_listening());
  CPPUNIT_ASSERT(bound.inet != 0 && bound.inet6 != 0);
  CPPUNIT_ASSERT(can_connect(AF_INET, bound.inet));
  CPPUNIT_ASSERT(can_connect(AF_INET6, bound.inet6));
  CPPUNIT_ASSERT(!can_connect(AF_INET, old));
  CPPUNIT_ASSERT(!can_connect(AF_INET6, old));
}

void
test_network_manager::test_listen_port_change_ipv4_only() {
  redirect_listen();
  auto manager = torrent::runtime::network_manager();
  torrent::runtime::network_config()->set_block_ipv6(true);
  torrent::runtime::initialize_network();
  uint16_t old = manager->listen_port();
  uint16_t blocked = old == 1 ? 2 : 1;
  uint16_t next = 1;
  while (next == old || next == blocked)
    ++next;
  bound_ports bound;
  redirect_bind(AF_INET, blocked, bound);

  CPPUNIT_ASSERT_THROW(manager->set_listen_port(blocked), torrent::input_error);
  CPPUNIT_ASSERT_EQUAL(old, manager->listen_port());
  CPPUNIT_ASSERT(can_connect(AF_INET, old));

  redirect_listen_failure(AF_INET);
  CPPUNIT_ASSERT_THROW(manager->set_listen_port(next), torrent::input_error);
  CPPUNIT_ASSERT_EQUAL(old, manager->listen_port());
  CPPUNIT_ASSERT(can_connect(AF_INET, old));

  redirect_listen();
  manager->set_listen_port(next);
  CPPUNIT_ASSERT_EQUAL(next, manager->listen_port());
  CPPUNIT_ASSERT(bound.inet != 0);
  CPPUNIT_ASSERT(can_connect(AF_INET, bound.inet));
  CPPUNIT_ASSERT(!can_connect(AF_INET, old));
}

void
test_network_manager::test_listen_port_change_before_network() {
  redirect_listen();
  auto manager = torrent::runtime::network_manager();
  CPPUNIT_ASSERT(manager->listen_open(10000, 20000));
  uint16_t next = manager->listen_port() + 1;
  manager->set_listen_port(next);
  CPPUNIT_ASSERT(!manager->is_listening());
  CPPUNIT_ASSERT_EQUAL(next, manager->listen_port());
  CPPUNIT_ASSERT(torrent::runtime::client_config()->listen_port_range() == std::make_pair(next, next));
}

void
test_network_manager::test_listen_port_change_with_proxy() {
  redirect_listen();
  auto manager = torrent::runtime::network_manager();
  torrent::runtime::initialize_network();
  uint16_t old = manager->listen_port();
  uint16_t next = old == 65535 ? old - 1 : old + 1;
  torrent::runtime::proxy_manager()->set_proxy_url("socks5://127.0.0.1:8080");
  CPPUNIT_ASSERT(torrent::runtime::network_config()->is_block_incoming());
  manager->set_listen_port(next);
  CPPUNIT_ASSERT(!manager->is_listening());
  CPPUNIT_ASSERT_EQUAL(next, manager->listen_port());
  CPPUNIT_ASSERT(torrent::runtime::client_config()->listen_port_range() == std::make_pair(next, next));
}
