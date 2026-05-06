#include "config.h"

#include "torrent/net/resolver.h"

#include <cassert>
#include <netdb.h>

#include "net/thread_net.h"
#include "net/dns_buffer.h"
#include "net/dns_cache.h"
#include "torrent/exceptions.h"
#include "torrent/net/socket_address.h"
#include "torrent/system/thread.h"

namespace torrent::net {

const char*
gai_enum_error(int status) {
  switch (status) {
  case EAI_AGAIN:    return "EAI_AGAIN";
  case EAI_BADFLAGS: return "EAI_BADFLAGS";
  case EAI_FAIL:     return "EAI_FAIL";
  case EAI_FAMILY:   return "EAI_FAMILY";
  case EAI_MEMORY:   return "EAI_MEMORY";
  case EAI_NONAME:   return "EAI_NONAME";
  case EAI_OVERFLOW: return "EAI_OVERFLOW";
  case EAI_SERVICE:  return "EAI_SERVICE";
  case EAI_SOCKTYPE: return "EAI_SOCKTYPE";
  case EAI_SYSTEM:   return "EAI_SYSTEM";
  default:           return "unknown";
  }
}

std::string
gai_enum_error_str(int status) {
  return std::string(gai_enum_error(status));
}

void
Resolver::init() {
  m_thread = torrent::system::Thread::self();

  assert(m_thread != nullptr && "Resolver::m_thread is nullptr.");
}

void
Resolver::resolve_both(void* requester, const std::string& hostname, int family, both_callback&& callback) {
  auto [sin, sin6] = try_lookup_numeric(hostname, family);

  if (sin || sin6) {
    m_thread->callback(requester, [family, callback = std::move(callback), sin = std::move(sin), sin6 = std::move(sin6)]() mutable {
        int err  = (!sin  && family == AF_UNSPEC) ? EAI_NONAME : 0;
        int err6 = (!sin6 && family == AF_UNSPEC) ? EAI_NONAME : 0;

        callback(std::move(sin), err, std::move(sin6), err6);
      });
    return;
  }

  auto cb = [this, requester, hostname, family, callback = std::move(callback)]() mutable {
      auto fn = [this, requester, callback = std::move(callback)](sin_shared_ptr sin, int err, sin6_shared_ptr sin6, int err6) mutable {
          m_thread->callback(requester, std::bind(std::move(callback), std::move(sin), err, std::move(sin6), err6));
        };

      ThreadNet::thread_net()->dns_cache()->resolve(requester, hostname, family, std::move(fn));
    };

  net_thread::callback(requester, std::move(cb));
}

void
Resolver::resolve_preferred(void* requester, const std::string& hostname, int family, int preferred, single_callback&& callback) {
  if (preferred != AF_INET && preferred != AF_INET6)
    throw internal_error("Resolver::resolve_preferred() invalid preferred family.");

  auto [sin, sin6] = try_lookup_numeric(hostname, family);

  if (sin || sin6) {
    sa_shared_ptr sa = sin ? sa_copy_in(sin.get()) : sa_copy_in6(sin6.get());

    m_thread->callback(requester, [callback = std::move(callback), sa = std::move(sa)]() mutable {
        callback(sa, 0);
      });
    return;
  }

  auto cb = [this, requester, hostname, family, preferred, callback = std::move(callback)] () mutable {
      auto fn = [this, requester, preferred, callback = std::move(callback)](const auto& sin, int err, const auto& sin6, int err6) mutable {
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

          m_thread->callback(requester, std::bind(std::move(callback), std::move(result), error));
        };

      ThreadNet::thread_net()->dns_cache()->resolve(requester, hostname, family, std::move(fn));
    };

  net_thread::callback(requester, std::move(cb));
}

void
Resolver::resolve_specific(void* requester, const std::string& hostname, int family, single_callback&& callback) {
  if (family != AF_INET && family != AF_INET6)
    throw internal_error("Resolver::resolve_specific() invalid family.");

  auto [sin, sin6] = try_lookup_numeric(hostname, family);

  if (sin || sin6) {
    sa_shared_ptr sa = sin ? sa_copy_in(sin.get()) : sa_copy_in6(sin6.get());

    m_thread->callback(requester, [callback = std::move(callback), sa = std::move(sa)]() mutable {
        callback(sa, 0);
      });
    return;
  }

  auto cb = [this, requester, hostname, family, callback = std::move(callback)]() mutable {
      auto fn = [this, requester, family, callback = std::move(callback)](const auto& sin, int err, const auto& sin6, int err6) mutable {
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

          m_thread->callback(requester, std::bind(std::move(callback), std::move(result), error));
        };

      ThreadNet::thread_net()->dns_cache()->resolve(requester, hostname, family, std::move(fn));
    };

  net_thread::callback(requester, std::move(cb));
}

void
Resolver::cancel(void* requester) {
  assert(m_thread != nullptr && std::this_thread::get_id() == m_thread->thread_id());

  // Make sure no new resolve requests get added.
  net_thread::cancel_callback_and_wait(requester);

  // No new resolve results will be added after this.
  ThreadNet::thread_net()->dns_buffer()->cancel_safe(requester);

  // Remove any new resolve results that got added before between the above two calls.
  net_thread::cancel_callback_and_wait(requester);

  // Self thread doesn't need to wait.
  m_thread->cancel_callback(requester);
}

} // namespace torrent::net
