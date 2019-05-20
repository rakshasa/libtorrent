#include "config.h"

#include "mock_function.h"

#include <fcntl.h>
#include <iostream>
#include <torrent/net/fd.h>

namespace torrent {
extern "C" {

  int fd__close(int fildes) {
    return mock_call<int>(&torrent::fd__close, fildes);
  }

  int fd__fcntl_int(int fildes, int cmd, int arg) {
    std::cout << std::endl << "fd__fcntl_int: " << fildes << ' ' << cmd << ' ' << arg;
    return fcntl(fildes, cmd, arg);
  }

  int fd__socket(int domain, int type, int protocol) {
    std::cout << std::endl << "fd__socket: " << domain << ' ' << type << ' ' << protocol;
    return mock_call<int>(&torrent::fd__socket, domain, type, protocol);
  }

}
}
