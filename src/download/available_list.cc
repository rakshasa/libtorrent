#include "config.h"

#include <algorithm>
#include <iterator>

#include "download/available_list.h"
#include "torrent/exceptions.h"
#include "torrent/net/socket_address.h"

namespace torrent {

AvailableList::value_type
AvailableList::pop_random() {
  if (empty())
    throw internal_error("AvailableList::pop_random() called on an empty container");

  size_type idx = random() % size();

  auto tmp = std::move(*(begin() + idx));

  *(begin() + idx) = std::move(back());
  pop_back();

  return tmp;
}

void
AvailableList::insert(AddressList* source_list) {
  if (!want_more())
    return;

  source_list->sort();
  std::sort(begin(), end(), [](auto& a, auto& b) { return sa_less(&a.sa, &b.sa); });

  // Can we use the std::remove* semantics for this, and just copy to 'l'?.
  //
  // 'l' is guaranteed to be sorted, so we can just do std::set_difference.

  AddressList difference;

  std::set_difference(source_list->begin(), source_list->end(),
                      begin(), end(),
                      std::back_inserter(difference),
                      [](auto& a, auto& b) { return sa_less(&a.sa, &b.sa); });

  base_type::insert(end(), difference.begin(), difference.end());
}

bool
AvailableList::insert_unique(const sockaddr* sa) {
  if (std::find_if(begin(), end(), [sa](auto& a) { return sa_equal(&a.sa, sa); }) != end())
    return false;

  base_type::push_back(sa_inet_union_from_sa(sa));
  return true;
}

} // namespace torrent
