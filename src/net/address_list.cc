#include "config.h"

#include "address_list.h"

#include <algorithm>
#include <arpa/inet.h>

#include "torrent/net/socket_address.h"

namespace torrent {

void
AddressList::sort() {
  std::sort(begin(), end(), [](auto& a, auto& b) { return sa_less(&a.sa, &b.sa); });
}

void
AddressList::sort_and_unique() {
  sort();
  erase(std::unique(begin(), end(), [](auto& a, auto& b) { return sa_equal(&a.sa, &b.sa); }),
        end());
}

void
AddressList::parse_address_normal(const Object::list_type& b) {
  for (const auto& obj : b) {
    if (!obj.is_map())
      continue;
    if (!obj.has_key_string("ip"))
      continue;
    if (!obj.has_key_value("port"))
      continue;

    auto& addr = obj.get_key_string("ip");
    auto port = obj.get_key_value("port");

    if (port <= 0 || port >= (1 << 16))
      continue;

    sa_inet_union sa{};

    if (inet_pton(AF_INET, addr.c_str(), &sa.inet.sin_addr)) {
      if (sa.inet.sin_addr.s_addr == htonl(INADDR_ANY))
        continue;

      sa.inet.sin_family = AF_INET;
      sa.inet.sin_port = htons(port);

    } else if (inet_pton(AF_INET6, addr.c_str(), &sa.inet6.sin6_addr)) {
      if (IN6_IS_ADDR_UNSPECIFIED(&sa.inet6.sin6_addr))
        continue;

      sa.inet6.sin6_family = AF_INET6;
      sa.inet6.sin6_port = htons(port);

    } else {
      continue;
    }

    this->push_back(sa);
  }
}

void
AddressList::parse_address_compact(raw_string s) {
  if (sizeof(const SocketAddressCompact) != 6)
    throw internal_error("ConnectionList::AddressList::parse_address_compact(...) bad struct size.");

  std::copy(reinterpret_cast<const SocketAddressCompact*>(s.data()),
            reinterpret_cast<const SocketAddressCompact*>(s.data() + s.size() - s.size() % sizeof(SocketAddressCompact)),
            std::back_inserter(*this));
}

void
AddressList::parse_address_compact_ipv6(const std::string& s) {
  if (sizeof(const SocketAddressCompact6) != 18)
    throw internal_error("ConnectionList::AddressList::parse_address_compact_ipv6(...) bad struct size.");

  std::copy(reinterpret_cast<const SocketAddressCompact6*>(s.c_str()),
            reinterpret_cast<const SocketAddressCompact6*>(s.c_str() + s.size() - s.size() % sizeof(SocketAddressCompact6)),
            std::back_inserter(*this));
}

void
AddressList::parse_address_bencode(raw_list s) {
  if (sizeof(const SocketAddressCompact) != 6)
    throw internal_error("AddressList::parse_address_bencode(...) bad struct size.");

  for (auto itr = s.begin(); itr + 2 + sizeof(SocketAddressCompact) <= s.end(); itr += sizeof(SocketAddressCompact)) {
    if (*itr++ != '6' || *itr++ != ':')
      break;

    insert(end(), *reinterpret_cast<const SocketAddressCompact*>(itr));
  }
}

} // namespace torrent
