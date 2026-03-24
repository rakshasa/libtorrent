#include "config.h"

#include "torrent/net/resolver.h"

#include <cassert>

#include "net/thread_net.h"
#include "net/dns_buffer.h"
#include "net/dns_cache.h"
#include "torrent/exceptions.h"
#include "torrent/net/socket_address.h"
#include "torrent/utils/thread.h"

namespace torrent::net {

void
Resolver::init() {
  m_thread = torrent::utils::Thread::self();

  assert(m_thread != nullptr && "Resolver::m_thread is nullptr.");
}

void
Resolver::resolve_both(void* requester, const std::string& hostname, int family, both_callback&& callback) {
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

  // While processing results, udns is locked so we need to cancel the callback before canceling the
  // request.

  net_thread::cancel_callback_and_wait(requester);

  ThreadNet::thread_net()->dns_buffer()->cancel_safe(requester);

  // TODO: This is wrong.

  // // DnsBuffer might have added callbacks if it locked DnsBufferRequester shared_ptr right after
  // // cancel_safe().
  // net_thread::cancel_callback_and_wait(requester);

  // Self thread doesn't need to wait.
  m_thread->cancel_callback(requester);
}

} // namespace torrent::net
