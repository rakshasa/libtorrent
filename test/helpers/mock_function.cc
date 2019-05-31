#include "config.h"

#include "mock_function.h"

#include <fcntl.h>
#include <iostream>
#include <torrent/net/socket_address.h>
#include <torrent/net/fd.h>
#include <torrent/utils/log.h>

#define MOCK_CLEANUP_MAP(MOCK_FUNC) \
  CPPUNIT_ASSERT_MESSAGE("expected mock function calls not completed for '" #MOCK_FUNC "'", mock_cleanup_map(&MOCK_FUNC) || ignore_assert);
#define MOCK_LOG(log_fmt, ...)                                          \
  lt_log_print(torrent::LOG_MOCK_CALLS, "%s: " log_fmt, __func__, __VA_ARGS__);

void
mock_clear(bool ignore_assert) {
  MOCK_CLEANUP_MAP(torrent::fd__socket);
  MOCK_CLEANUP_MAP(torrent::fd__fcntl_int);
};

void mock_init() {
  log_add_group_output(torrent::LOG_MOCK_CALLS, "test_output");

  mock_clear(true);
}

void mock_cleanup() {
  mock_clear(false);
}

namespace torrent {
extern "C" {

  int fd__close(int fildes) {
    MOCK_LOG("filedes:%i", fildes);
    return mock_call<int>(__func__, &torrent::fd__close, fildes);
  }

  int fd__connect(int socket, const sockaddr *address, socklen_t address_len) {
    MOCK_LOG("socket:%i address:%s address_len:%u", socket, torrent::sa_pretty_str(address).c_str(), (unsigned int)address_len);
    return mock_call<int>(__func__, &torrent::fd__connect, socket, address, address_len);
  }

  int fd__fcntl_int(int fildes, int cmd, int arg) {
    MOCK_LOG("filedes:%i cmd:%i arg:%i", fildes, cmd, arg);
    return mock_call<int>(__func__, &torrent::fd__fcntl_int, fildes, cmd, arg);
  }

  int fd__socket(int domain, int type, int protocol) {
    MOCK_LOG("domain:%i type:%i protocol:%i", domain, type, protocol);
    return mock_call<int>(__func__, &torrent::fd__socket, domain, type, protocol);
  }

}
}
