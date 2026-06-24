#include "config.h"

#include "net/proxy/proxy_manager.h"

#include "net/proxy/proxy.h"
#include "net/proxy/proxy_http.h"
#include "torrent/exceptions.h"
#include "torrent/net/types.h"

namespace torrent::net::proxy {

ProxyManager::ProxyManager() {
  m_create_proxy = []() { return nullptr; };
}

std::string
ProxyManager::address() {
  auto guard = lock_guard();
  return m_address;
}

void
ProxyManager::set_address(const std::string& address) {
  auto guard = lock_guard();

  auto schema = parse_uri_scheme(address);

  auto [host, port]     = parse_uri_host_port(address);
  auto [user, password] = parse_uri_user_password(address);

  // TODO: Verify there's no junk after the port number?

  if (schema.empty())
    throw input_error("Proxy address must include a scheme.");

  if (host.empty())
    throw input_error("Proxy address must include a host.");

  if (port == 0)
    throw input_error("Proxy address must include a port.");

  auto verify_no_user_password = [user, password]() {
      if (!user.empty() || !password.empty())
        throw input_error("Proxy address must not include a user or password.");
    };

  if (schema == "http") {
    verify_no_user_password();

    m_create_proxy = [host, port]() {
        return new ProxyHttp(host, port);
      };

  // } else if (schema == "https") {
  //   m_create_proxy = [host, port, user, password]() {
  //     // return std::make_unique<Proxy>(host, port, user, password);
  //     return nullptr;
  //   };

  // } else if (schema == "socks4") {
  //   m_create_proxy = [host, port, user, password]() {
  //     // return std::make_unique<Proxy>(host, port, user, password);
  //     return nullptr;
  //   };

  // } else if (schema == "socks4a") {
  //   m_create_proxy = [host, port, user, password]() {
  //     // return std::make_unique<Proxy>(host, port, user, password);
  //     return nullptr;
  //   };

  // } else if (schema == "socks5") {
  //   m_create_proxy = [host, port, user, password]() {
  //     // return std::make_unique<Proxy>(host, port, user, password);
  //     return nullptr;
  //   };

  // } else if (schema == "socks5h") {
  //   m_create_proxy = [host, port, user, password]() {
  //     // return std::make_unique<Proxy>(host, port, user, password);
  //     return nullptr;
  //   };

  } else {
    throw input_error("Unsupported proxy scheme: " + schema);
  }

  m_address = address;
}

std::unique_ptr<Proxy>
ProxyManager::create_proxy() {
  auto guard = lock_guard();

  if (m_create_proxy == nullptr)
    return nullptr;

  auto proxy = m_create_proxy();

  if (proxy == nullptr)
    throw internal_error("ProxyManager::create_proxy() failed to create a proxy object.");

  return std::unique_ptr<Proxy>(proxy);
}

} // namespace torrent::net::proxy
