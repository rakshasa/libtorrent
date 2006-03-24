#include <iostream>
#include <iomanip>
#include <cstdlib>
#include <rak/partial_queue.h>
#include <rak/timer.h>

int
main(int argc, char** argv) {
  srand(rak::timer::current().usec());

  std::cout << "sizeof(rak::partial_queue): " << sizeof(rak::partial_queue) << std::endl;

  rak::partial_queue queue;
  queue.resize(8);
  queue.clear();

  std::cout << "Inserting: " << std::endl;

  for (int i = 0; i < 10 && !queue.is_full(); ++i) {
    uint8_t k = rand() % 256;

    if (queue.insert(k, k))
      std::cout << '[' << (uint32_t)k << ']' << ' ';
    else
      std::cout << '<' << (uint32_t)k << '>' << ' ';
  }

  std::cout << std::endl
	    << "Popping:" << std::endl;

  while (queue.prepare_pop()) {
    if (rand() % 2) {
      std::cout << queue.pop() << ' ';
    } else {
      uint8_t k = rand() % 128;

      if (queue.insert(k, k))
	std::cout << '[' << (uint32_t)k << ']' << ' ';
      else
	std::cout << '<' << (uint32_t)k << '>' << ' ';
    }
  }

  std::cout << std::endl;

  return 0;
}
