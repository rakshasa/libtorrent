#include "config.h"

#include "test_listen.h"

#include <fcntl.h>
#include <sys/resource.h>
#include <unistd.h>

#include "helpers/mock_function.h"
#include "net/listen.h"
#include "torrent/exceptions.h"
#include "torrent/net/fd.h"
#include "torrent/net/socket_address.h"
#include "torrent/utils/functional.h"

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(test_listen, "net");

namespace {
using namespace torrent;

struct listeners {
  Listen        inet;
  Listen        inet6;
  sa_unique_ptr address  = sa_make_inet_h(INADDR_LOOPBACK, 51000);
  sa_unique_ptr address6 = sa_make_inet6_any();

  Listen::open_options options{51000, 51000, SOMAXCONN, true, false};
  std::vector<int> expected_closes;
  std::vector<int> close_results;
  std::vector<int> actual_closes;

  listeners() {
    reinterpret_cast<sockaddr_in6*>(address6.get())->sin6_addr = in6addr_loopback;
    sa_set_port(address6.get(), 51000);
    mock_redirect(fd__close, std::function<int(int)>([this](int fd) {
        auto index = actual_closes.size();
        actual_closes.push_back(fd);
        return index < close_results.size() ? close_results[index] : 0;
      }));
  }

  void expect_close(int fd, int result = 0) {
    expected_closes.push_back(fd);
    close_results.push_back(result);
  }

  bool open() {
    return Listen::open_both({&inet, address.get(), ""}, {&inet6, address6.get(), ""}, options);
  }

  void assert_closed() {
    CPPUNIT_ASSERT(actual_closes == expected_closes);
    CPPUNIT_ASSERT(!inet.is_open());
    CPPUNIT_ASSERT(!inet6.is_open());
    CPPUNIT_ASSERT_EQUAL(uint16_t(0), inet.port());
    CPPUNIT_ASSERT_EQUAL(uint16_t(0), inet6.port());
  }
};

void
expect_open(int fd, int family, int type) {
  mock_expect(&fd__socket, fd, family, type, type == SOCK_STREAM ? int(IPPROTO_TCP) : int(IPPROTO_UDP));
  if (family == PF_INET6)
    mock_expect(&fd__setsockopt_int, 0, fd, int(IPPROTO_IPV6), int(IPV6_V6ONLY), int(true));
  mock_expect(&fd__fcntl_int, 0, fd, F_GETFL, 0);
  mock_expect(&fd__fcntl_int, 0, fd, F_SETFL, O_NONBLOCK);
  mock_expect(&fd__setsockopt_int, 0, fd, int(SOL_SOCKET), int(SO_REUSEADDR), int(true));
}

void
expect_range(listeners& pair, int fd, bool ipv6, bool bound = true) {
  bool check_dht = pair.options.check_dht;
  expect_open(fd, ipv6 ? PF_INET6 : PF_INET, SOCK_STREAM);
  if (check_dht)
    expect_open(fd + 1, ipv6 ? PF_INET6 : PF_INET, SOCK_DGRAM);
  const sockaddr* address = ipv6 ? pair.address6.get() : pair.address.get();
  mock_expect(&fd__bind, bound ? 0 : -1, fd, address, socklen_t(sa_length(address)));
  if (bound && check_dht)
    mock_expect(&fd__bind, 0, fd + 1, address, socklen_t(sa_length(address)));
  if (!bound)
    pair.expect_close(fd);
  if (check_dht)
    pair.expect_close(fd + 1);
}
}

void
test_listen::test_second_stream_failure() {
  listeners pair;
  expect_range(pair, 100, false);
  mock_expect(&torrent::fd__socket, -1, int(PF_INET6), int(SOCK_STREAM), int(IPPROTO_TCP));
  pair.expect_close(100, 0);
  CPPUNIT_ASSERT_THROW(pair.open(), torrent::resource_error);
  pair.assert_closed();
}

void
test_listen::test_second_datagram_failure() {
  listeners pair;
  pair.options.check_dht = true;
  expect_range(pair, 100, false);
  expect_open(102, PF_INET6, SOCK_STREAM);
  mock_expect(&torrent::fd__socket, -1, int(PF_INET6), int(SOCK_DGRAM), int(IPPROTO_UDP));
  pair.expect_close(102, 0);
  pair.expect_close(100, 0);
  CPPUNIT_ASSERT_THROW(pair.open(), torrent::resource_error);
  pair.assert_closed();
}

void
test_listen::test_first_stream_failure() {
  listeners pair;
  mock_expect(&torrent::fd__socket, -1, int(PF_INET), int(SOCK_STREAM), int(IPPROTO_TCP));
  CPPUNIT_ASSERT_THROW(pair.open(), torrent::resource_error);
  pair.assert_closed();
}

void
test_listen::test_listen_failure() {
  for (bool second : {false, true}) {
    listeners pair;
    expect_range(pair, 100, false);
    expect_range(pair, 102, true);
    mock_expect(&torrent::fd__listen, second ? 0 : -1, 100, SOMAXCONN);
    if (second)
      mock_expect(&torrent::fd__listen, -1, 102, SOMAXCONN);
    pair.expect_close(100, 0);
    pair.expect_close(102, 0);
    CPPUNIT_ASSERT_THROW(pair.open(), torrent::resource_error);
    pair.assert_closed();
  }
}

void
test_listen::test_retry_failure() {
  listeners pair;
  pair.options.last_port++;
  expect_range(pair, 100, false);
  expect_range(pair, 102, true, false);
  pair.expect_close(100, 0);
  mock_expect(&torrent::fd__socket, -1, int(PF_INET), int(SOCK_STREAM), int(IPPROTO_TCP));
  CPPUNIT_ASSERT_THROW(pair.open(), torrent::resource_error);
  pair.assert_closed();
}

void
test_listen::test_range_exhausted() {
  for (bool ipv4_bound : {false, true}) {
    listeners pair;
    expect_range(pair, 100, false, ipv4_bound);
    if (ipv4_bound) {
      expect_range(pair, 102, true, false);
      pair.expect_close(100, 0);
    }
    CPPUNIT_ASSERT(!pair.open());
    pair.assert_closed();
  }
}

void
test_listen::test_dht_port_unavailable() {
  listeners pair;
  pair.options.check_dht  = true;
  const sockaddr* address = pair.address.get();
  expect_open(100, PF_INET, SOCK_STREAM);
  expect_open(101, PF_INET, SOCK_DGRAM);
  mock_expect(&torrent::fd__bind, 0, 100, address, socklen_t(sa_length(address)));
  mock_expect(&torrent::fd__bind, -1, 101, address, socklen_t(sa_length(address)));
  expect_open(102, PF_INET, SOCK_STREAM);
  expect_open(103, PF_INET, SOCK_DGRAM);
  for (int fd : { 100, 101, 102, 103 })
    pair.expect_close(fd);
  CPPUNIT_ASSERT(!pair.open());
  pair.assert_closed();
}

void
test_listen::test_device_failure() {
  for (bool check_dht : { false, true }) {
    listeners pair;
    pair.options.check_dht = check_dht;
    expect_open(100, PF_INET, SOCK_STREAM);
    if (check_dht)
      expect_open(101, PF_INET, SOCK_DGRAM);
    pair.expect_close(100);
    if (check_dht)
      pair.expect_close(101);
    CPPUNIT_ASSERT_THROW(Listen::open_single({ &pair.inet, pair.address.get(), "lo" }, pair.options), torrent::resource_error);
    pair.assert_closed();
  }
}

void
test_listen::test_invalid_address() {
  listeners pair;
  pair.address6 = sa_make_unix("");
  CPPUNIT_ASSERT_THROW(pair.open(), torrent::input_error);
  pair.assert_closed();
}

void
test_listen::test_open_success() {
  listeners pair;
  mock_redirect_defaults();
  mock_redirect(torrent::fd__listen, std::function<int(int, int)>([](int fd, int backlog) { return ::listen(fd, backlog); }));
  bool retried = false;
  mock_redirect(torrent::fd__bind, std::function<int(int, const sockaddr*, socklen_t)>([&retried](int fd, const sockaddr* address, socklen_t length) {
      if (address->sa_family == AF_INET6 && !retried) {
        retried = true;
        errno = EADDRINUSE;
        return -1;
      }
      return ::bind(fd, address, length);
    }));
  pair.options.last_port += 100;
  CPPUNIT_ASSERT(pair.open());
  CPPUNIT_ASSERT(pair.inet.is_open());
  CPPUNIT_ASSERT(pair.inet6.is_open());
  CPPUNIT_ASSERT(retried);
  CPPUNIT_ASSERT(pair.inet.port() > pair.options.first_port);
  CPPUNIT_ASSERT_EQUAL(pair.inet.port(), pair.inet6.port());
  pair.inet.close();
  pair.inet6.close();
  pair.assert_closed();
}

void
test_listen::test_reopen_at_descriptor_limit() {
  listeners pair;
  mock_redirect_defaults();
  mock_redirect(torrent::fd__listen, std::function<int(int, int)>([](int fd, int backlog) { return ::listen(fd, backlog); }));
  pair.options.last_port += 100;
  CPPUNIT_ASSERT(pair.open());

  // A listen restart closes both listeners and reopens them on the same port. At the descriptor
  // limit only the descriptors the close released are available.
  pair.options.first_port = pair.options.last_port = pair.inet.port();

  rlimit saved{};
  CPPUNIT_ASSERT(getrlimit(RLIMIT_NOFILE, &saved) == 0);

  int lowest_free = ::open("/dev/null", O_RDONLY);
  CPPUNIT_ASSERT(lowest_free != -1);
  ::close(lowest_free);

  rlimit limited   = saved;
  limited.rlim_cur = lowest_free;

  torrent::utils::scope_guard restore([&saved]() { setrlimit(RLIMIT_NOFILE, &saved); });
  CPPUNIT_ASSERT(setrlimit(RLIMIT_NOFILE, &limited) == 0);

  pair.inet.close();
  pair.inet6.close();

  CPPUNIT_ASSERT(pair.open());
  CPPUNIT_ASSERT(pair.inet.is_open());
  CPPUNIT_ASSERT(pair.inet6.is_open());
  pair.inet.close();
  pair.inet6.close();
}
