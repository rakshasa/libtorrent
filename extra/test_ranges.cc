#include <cassert>
#include "../src/utils/ranges.h"

int main() {
  torrent::Ranges r;

  r.insert(10, 20);
  r.insert(30, 40);
  
  assert(r.has(10) && !r.has(20) && r.has(30) && !r.has(40));

  r.insert(0, 5);

  assert(r.has(2) && !r.has(5) && r.has(10));

  r.insert(35, 60);

  assert(r.has(30) && r.has(55) && !r.has(60));

  r.insert(0, 55);

  assert(r.has(8) && r.has(25) && r.size() == 1);

  return 0;
}

	 
  
