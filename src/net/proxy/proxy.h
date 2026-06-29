#ifndef LIBTORRENT_NET_PROXY_PROXY_H
#define LIBTORRENT_NET_PROXY_PROXY_H

#include "torrent/common.h"
#include "torrent/net/types.h"

namespace torrent::net::proxy {

enum {
  state_writing,
  state_reading,
  state_done,
  state_error
};

class Proxy {
public:
  // virtual ~Proxy() = 0;
  virtual ~Proxy();

  const auto*         proxy_address() const;

  virtual int         next_action() = 0;

  // Returns the number of bytes consumed, or 0 if no bytes were consumed.
  virtual uint32_t    read(const void* data, uint32_t size) = 0;
  virtual uint32_t    write(void* data, uint32_t max_size) = 0;

protected:
  sa_inet_union       m_proxy_address{};
};

inline const auto* Proxy::proxy_address() const { return &m_proxy_address.sa; }

} // namespace torrent::net::proxy

#endif
