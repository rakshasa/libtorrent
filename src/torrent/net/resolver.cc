#include "config.h"

#include "torrent/net/resolver.h"

#include <cassert>
#include <netdb.h>

#include "net/thread_net.h"
#include "net/dns_buffer.h"
#include "net/dns_cache.h"
#include "torrent/exceptions.h"
#include "torrent/net/socket_address.h"
#include "torrent/system/callbacks.h"
#include "torrent/system/thread.h"

namespace torrent::net {

void
Resolver::init() {
  m_thread = torrent::system::Thread::self();

  assert(m_thread != nullptr && "Resolver::m_thread is nullptr.");
}

void
Resolver::resolve_both(system::callback_id& id, const std::string& hostname, int family, both_callback&& callback) {
  auto [sin, sin6] = try_lookup_numeric(hostname, family);

  if (sin || sin6) {
    m_thread->callback(id, [family, callback = std::move(callback), sin = std::move(sin), sin6 = std::move(sin6)]() mutable {
        int err  = (!sin  && family == AF_UNSPEC) ? EAI_NONAME : 0;
        int err6 = (!sin6 && family == AF_UNSPEC) ? EAI_NONAME : 0;

        callback(std::move(sin), err, std::move(sin6), err6);
      });
    return;
  }

  auto cb = [this, id, hostname, family, callback = std::move(callback)]() mutable {
      auto fn = [this, id, callback = std::move(callback)](sin_shared_ptr sin, int err, sin6_shared_ptr sin6, int err6) mutable {
          m_thread->callback(id, std::bind(std::move(callback), std::move(sin), err, std::move(sin6), err6));
        };

      ThreadNet::thread_net()->dns_cache()->resolve(nullptr, hostname, family, std::move(fn));
    };

  net_thread::callback(id, std::move(cb));
}

void
Resolver::resolve_preferred(system::callback_id& id, const std::string& hostname, int family, int preferred, single_callback&& callback) {
  if (preferred != AF_INET && preferred != AF_INET6)
    throw internal_error("Resolver::resolve_preferred() invalid preferred family.");

  auto [sin, sin6] = try_lookup_numeric(hostname, family);

  if (sin || sin6) {
    sa_shared_ptr sa = sin ? sa_copy_in(sin.get()) : sa_copy_in6(sin6.get());

    m_thread->callback(id, [callback = std::move(callback), sa = std::move(sa)]() mutable {
        callback(sa, 0);
      });
    return;
  }

  auto cb = [this, id, hostname, family, preferred, callback = std::move(callback)] () mutable {
      auto fn = [this, id, preferred, callback = std::move(callback)](const auto& sin, int err, const auto& sin6, int err6) mutable {
          // 'result' copies the deleter from the unique_ptr returned by sa_copy_in.
          sa_shared_ptr result;
          int           error{};

          // TODO: Do we need to handle no-record differently?

          if (preferred == AF_INET) {
            if (err == 0 && sin != nullptr)
              result = sa_copy_in(sin.get());
            else if (err6 == 0 && sin6 != nullptr)
              result = sa_copy_in6(sin6.get());
            else
              error = err != 0 ? err : err6;

          } else {
            if (err6 == 0 && sin6 != nullptr)
              result = sa_copy_in6(sin6.get());
            else if (err == 0 && sin != nullptr)
              result = sa_copy_in(sin.get());
            else
              error = err6 != 0 ? err6 : err;
          }

          m_thread->callback(id, std::bind(std::move(callback), std::move(result), error));
        };

      ThreadNet::thread_net()->dns_cache()->resolve(nullptr, hostname, family, std::move(fn));
    };

  net_thread::callback(id, std::move(cb));
}

void
Resolver::resolve_specific(system::callback_id& id, const std::string& hostname, int family, single_callback&& callback) {
  if (family != AF_INET && family != AF_INET6)
    throw internal_error("Resolver::resolve_specific() invalid family.");

  auto [sin, sin6] = try_lookup_numeric(hostname, family);

  if (sin || sin6) {
    sa_shared_ptr sa = sin ? sa_copy_in(sin.get()) : sa_copy_in6(sin6.get());

    m_thread->callback(id, [callback = std::move(callback), sa = std::move(sa)]() mutable {
        callback(sa, 0);
      });
    return;
  }

  auto cb = [this, id, hostname, family, callback = std::move(callback)]() mutable {
      auto fn = [this, id, family, callback = std::move(callback)](const auto& sin, int err, const auto& sin6, int err6) mutable {
          sa_shared_ptr result;
          int           error{};

          if (family == AF_INET) {
            if (err == 0 && sin != nullptr)
              result = sa_copy_in(sin.get());
            else
              error = err;

          } else {
            if (err6 == 0 && sin6 != nullptr)
              result = sa_copy_in6(sin6.get());
            else
              error = err6;
          }

          m_thread->callback(id, std::bind(std::move(callback), std::move(result), error));
        };

      ThreadNet::thread_net()->dns_cache()->resolve(nullptr, hostname, family, std::move(fn));
    };

  net_thread::callback(id, std::move(cb));
}

void
Resolver::cancel(system::callback_id& id) {
  assert(m_thread != nullptr && std::this_thread::get_id() == m_thread->thread_id());

  system::cancel_callback_and_wait(id, m_thread, net_thread::thread());
}

} // namespace torrent::net
