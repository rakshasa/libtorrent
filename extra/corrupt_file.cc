#include <iostream>
#include <stdexcept>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

#include "../rak/file_stat.h"

void
corrupt_region(int fd, int pos, int length) {
  char buf[length];

  for (char *first = buf, *last = buf + length; first != last; ++first)
    *first = '\0';

  if (write(fd, buf, length) == -1)
    throw std::runtime_error("Could not write data to file.");
}

int
main(int argc, char** argv) {
  if (argc != 4)
    throw std::runtime_error("Too few arguments.");

  int seed;
  int length;

  if (sscanf(argv[1], "%i", &seed) != 1)
    throw std::runtime_error("Could not parse seed.");

  if (sscanf(argv[2], "%i", &length) != 1)
    throw std::runtime_error("Could not parse length.");

  int fd = open(argv[3], O_RDWR);

  if (fd == -1)
    throw std::runtime_error("Could not open file.");

  rak::file_stat fileStat;

  if (!fileStat.update(fd))
    throw std::runtime_error("Could not read fs stat.");

  srand(seed);
  
  corrupt_region(fd, rand() % (fileStat.size() - length), length);

  return 0;
}
