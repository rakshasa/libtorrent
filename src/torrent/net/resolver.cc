#include "config.h"

#include "torrent/net/resolver.h"

#include <cassert>

#include "net/thread_net.h"
#include "net/udns_resolver.h"
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
      auto fn = [this, requester, callback = std::move(callback)](sin_shared_ptr sin, sin6_shared_ptr sin6, int err) mutable {
          m_thread->callback(requester, std::bind(std::move(callback), std::move(sin), std::move(sin6), err));
        };

      ThreadNet::thread_net()->udns()->resolve(requester, hostname, family, std::move(fn));
    };

  net_thread::callback(requester, std::move(cb));
}

void
Resolver::resolve_preferred(void* requester, const std::string& hostname, int family, int preferred, single_callback&& callback) {
  if (preferred != AF_INET && preferred != AF_INET6)
    throw internal_error("Invalid preferred family.");

  auto cb = [this, requester, hostname, family, preferred, callback = std::move(callback)] () mutable {
      auto fn = [this, requester, preferred, callback = std::move(callback)](const auto& sin, const auto& sin6, int err) mutable {
          // 'result' copies the deleter from the unique_ptr returned by sa_copy_in.
          sa_shared_ptr result;

          if (err == 0) {
            if (sin != nullptr && sin6 != nullptr) {
              if (preferred == AF_INET)
                result = sa_copy_in(sin.get());
              else
                result = sa_copy_in6(sin6.get());

            } else if (sin != nullptr) {
              result = sa_copy_in(sin.get());

            } else if (sin6 != nullptr) {
              result = sa_copy_in6(sin6.get());
            }
          }

          m_thread->callback(requester, std::bind(std::move(callback), std::move(result), err));
        };

      ThreadNet::thread_net()->udns()->resolve(requester, hostname, family, std::move(fn));
    };

  net_thread::callback(requester, std::move(cb));
}

void
Resolver::resolve_specific(void* requester, const std::string& hostname, int family, single_callback&& callback) {
  auto cb = [this, requester, hostname, family, callback = std::move(callback)]() mutable {
      auto fn = [this, requester, family, callback = std::move(callback)](const auto& sin, const auto& sin6, int err) mutable {
          sa_shared_ptr result;

          if (err == 0) {
            if (family == AF_INET && sin != nullptr)
              result = sa_copy_in(sin.get());

            if (family == AF_INET6 && sin6 != nullptr)
              result = sa_copy_in6(sin6.get());
          }

          m_thread->callback(requester, std::bind(std::move(callback), std::move(result), err));
        };

      ThreadNet::thread_net()->udns()->resolve(requester, hostname, family, std::move(fn));
    };

  net_thread::callback(requester, std::move(cb));
}

void
Resolver::cancel(void* requester) {
  assert(m_thread != nullptr && std::this_thread::get_id() == m_thread->thread_id());

  // While processing results, udns is locked so we only need to cancel the callback before
  // canceling the request.
  net_thread::cancel_callback_and_wait(requester);
  ThreadNet::thread_net()->udns()->cancel(requester);
  m_thread->cancel_callback_and_wait(requester);
}

} // namespace torrent::net
