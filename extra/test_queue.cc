#include <iostream>
#include <cstdlib>
#include <rak/priority_queue_default.h>

rak::priority_queue_default queue;//(priority_compare(), priority_equal(), priority_erase());
rak::priority_item items[100];

int last = 0;

void
print_item(rak::priority_item* p) {
  std::cout << 0 //p->second
	    << ' ' << p->time().usec() << std::endl;

  if (p->time().usec() < last) {
    std::cout << "order bork." << std::endl;
    exit(-1);
  }

  last = p->time().usec();

  if (std::rand() % 5) {
    int i = rand() % 100;

    std::cout << "erase " << i << ' ' << items[i].time().usec() << std::endl;
    queue.erase(items + i);
  }
}

int
main() {
  for (rak::priority_item* first = items, *last = items + 100; first != last; ++first) {
    //first->second = first - items;

    first->set_timer(std::rand() % 50);
    queue.push(first);
  }

  std::vector<rak::priority_item*> due;

  std::copy(rak::queue_popper(queue, rak::priority_ready(20)), rak::queue_popper(), std::back_inserter(due));
  std::for_each(due.begin(), due.end(), std::ptr_fun(&print_item));

  return 0;
}
