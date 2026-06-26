#include "config.h"

#include "torrent/runtime/proxy_manager.h"

#include <cassert>

#include "net/proxy/proxy.h"
#include "net/proxy/proxy_http.h"
#include "torrent/exceptions.h"
#include "torrent/net/socket_address.h"

namespace torrent::runtime {

ProxyManager::ProxyManager() {
  m_create_proxy = [](auto*) { return nullptr; };
}

void
ProxyManager::set_proxy_url(const std::string& url) {
  if (url.empty()) {
    auto guard = lock_guard();

    m_has_proxy    = false;
    m_blocks_udp   = false;
    m_proxy_url    = "";
    m_create_proxy = nullptr;
    return;
  }

  auto schema           = net::parse_uri_scheme(url);
  auto [host, port]     = net::parse_uri_host_port(url);
  auto [user, password] = net::parse_uri_user_password(url);

  // TODO: Verify there's no junk after the port number?

  if (schema.empty())
    throw input_error("Proxy address must include a scheme.");

  if (host.empty())
    throw input_error("Proxy address must include a host.");

  if (port == 0)
    throw input_error("Proxy address must include a port.");

  if (!net::verify_no_path_query_fragment(url))
    throw input_error("Proxy address must not include a path, query, or fragment.");

  auto verify_no_user_password = [schema, user, password]() {
      if (!user.empty() || !password.empty())
        throw input_error("Proxy address for '" + schema + "://' must not include a user or password.");
    };

  auto [proxy_sa, lookup_succeeded] = sa_lookup_numeric(host, AF_UNSPEC);

  if (!lookup_succeeded)
    throw input_error("Proxy address numeric lookup failed: " + host);

  create_proxy_func create_proxy_fn;

  auto proxy_union = sa_inet_union_from_sap(proxy_sa);

  if (schema == "http") {
    verify_no_user_password();

    create_proxy_fn = [proxy_union](auto* connect_sa) {
        return new net::proxy::ProxyHttp(&proxy_union.sa, sa_addr_str(connect_sa), sa_port(connect_sa));
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

  auto guard = lock_guard();

  m_has_proxy    = true;
  m_blocks_udp   = true;
  m_proxy_url    = url;
  m_create_proxy = create_proxy_fn;
}

void
ProxyManager::set_http_proxy_url(const std::string& url) {
  auto schema = net::parse_uri_scheme(url);

  if (schema != "http" &&
      schema != "https" &&
      schema != "socks4" &&
      schema != "socks4a" &&
      schema != "socks5" &&
      schema != "socks5h")
    throw input_error("Unsupported proxy scheme: " + schema);

  auto guard = lock_guard();

  m_http_proxy_url = url;
}


net::proxy_ptr
ProxyManager::create_proxy(const sockaddr* sa) {
  if (!m_has_proxy)
    return nullptr;

  assert(sa != nullptr);
  assert(sa->sa_family == AF_INET || sa->sa_family == AF_INET6);
  assert(!sa_is_any(sa) && sa_port(sa) != 0);

  auto guard = lock_guard();

  if (m_create_proxy == nullptr)
    return nullptr;

  return net::proxy_ptr(m_create_proxy(sa));
}

} // namespace torrent::net::proxy
