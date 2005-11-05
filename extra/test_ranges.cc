#include <cassert>
#include <iostream>
#include "../rak/ranges.h"

void print_ranges(rak::ranges<uint32_t>& r) {
  rak::ranges<uint32_t>::iterator itr = r.begin();

  std::cout << std::endl;

  while (itr != r.end()) {
    std::cout << itr->first << ' ' << itr->second << std::endl;

    ++itr;
  }
}

int main() {
  rak::ranges<uint32_t> r;

  r.insert(10, 20);
  r.insert(30, 40);
  r.insert(50, 60);
  assert(r.has(10) && !r.has(20) && r.has(30) && !r.has(40));

  r.insert(0, 5);
  assert(r.has(2) && !r.has(5) && r.has(10));

  r.insert(5, 30);
  assert(r.has(0) && r.has(7) && r.has(29) && r.has(30) && !r.has(40));

  print_ranges(r);

//   r.erase(15, 55);

//   print(r);

//   assert(r.size() == 3);

//   assert(r.has(14));
//   assert(!r.has(15));
//   assert(!r.has(54));
//   assert(r.has(55));

//   r.insert(5, 60);

//   print(r);

  return 0;
}

	 
  
