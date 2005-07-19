#include <fcntl.h>
#include <iostream>
#include "../src/utils/timer.h"

namespace torrent {
  int64_t Timer::m_cache;
}

#define FILE_SIZE ((off_t)1 << 20)

int main() {
  int fd = open("./posix_fallocate.out", O_CREAT | O_RDWR);

  if (ftruncate(fd, FILE_SIZE)) {
    std::cout << "truncate failed" << std::endl;
    return -1;
  }    

  torrent::Timer t1 = torrent::Timer::current();
  int v = posix_fallocate(fd, 0, FILE_SIZE);
  torrent::Timer t2 = torrent::Timer::current();

  std::cout << "Return: " << v << " Time: " << (t2 - t1).usec() << std::endl;

  return 0;
}
