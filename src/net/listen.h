#ifndef LIBTORRENT_LISTEN_H
#define LIBTORRENT_LISTEN_H

#include <cinttypes>
#include <functional>
#include <memory>

#include "torrent/system/event.h"

namespace torrent {

class Handshake;

class Listen : public system::Event {
public:
  using listen_info = std::tuple<Listen*, const sockaddr*, std::string>;

  struct open_options {
    uint16_t        first_port{};
    uint16_t        last_port{};
    int             backlog{};
    bool            block_ipv4in6{};
    bool            check_dht{};
  };

  ~Listen() override { close(); }

  const char*         type_name() const override { return "listen"; }

  static bool         open_single(listen_info listen, open_options options);
  static bool         open_both(listen_info listen4, listen_info listen6, open_options options);

  void                close();

  uint16_t            port() const    { return m_port; }

  auto&               slot_accepted() { return m_slot_accepted; }

  void                event_read() override;
  void                event_write() override;
  void                event_error() override;

private:
  using slot_accept_type = std::function<void(std::unique_ptr<Handshake>& handshake, int, const sockaddr*)>;

  void                open_done(int fd, uint16_t port, int backlog);

  uint16_t            m_port{0};
  slot_accept_type    m_slot_accepted;
};

} // namespace torrent

#endif // LIBTORRENT_TORRENT_H
