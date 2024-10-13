#include "config.h"

#include <algorithm>

#include "address_list.h"

namespace torrent {

void
AddressList::parse_address_normal(const Object::list_type& b) {
  std::for_each(b.begin(), b.end(), [=](auto& obj) {
      if (!obj.is_map())
        return;
      if (!obj.has_key_string("ip"))
        return;
      if (!obj.has_key_value("port"))
        return;

      rak::socket_address sa;
      sa.clear();

      if (!sa.set_address_str(obj.get_key_string("ip")))
        return;

      auto port = obj.get_key_value("port");

      if (port <= 0 || port >= (1 << 16))
        return;

      sa.set_port(port);

      if (sa.is_valid())
        this->push_back(sa);
    });
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

  for (raw_list::const_iterator itr = s.begin();
       itr + 2 + sizeof(SocketAddressCompact) <= s.end();
       itr += sizeof(SocketAddressCompact)) {
    if (*itr++ != '6' || *itr++ != ':')
      break;

    insert(end(), *reinterpret_cast<const SocketAddressCompact*>(s.data()));
  }
}

}
