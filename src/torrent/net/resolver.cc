#include "config.h"

#include "torrent/net/resolver.h"

#include "net/thread_net.h"
#include "net/udns_event.h"
#include "torrent/net/socket_address.h"
#include "torrent/utils/thread.h"

namespace torrent::net {

void
Resolver::init() {
  m_thread = torrent::thread_self;
}

void
Resolver::resolve_both(void* requester, const std::string& hostname, int family, both_callback&& callback) {
  thread_net->callback(requester, [this, requester, hostname, family, callback = std::move(callback)]() {
      thread_net->udns()->resolve(requester, hostname, family, [this, requester, callback = std::move(callback)](sin_shared_ptr sin, sin6_shared_ptr sin6, int err) {
          m_thread->callback(requester, [sin, sin6, err, callback = std::move(callback)]() {
              callback(sin, sin6, err);
            });
        });
    });
}

void
Resolver::resolve_preferred(void* requester, const std::string& hostname, int family, int preferred, single_callback&& callback) {
  if (preferred != AF_INET && preferred != AF_INET6)
    throw internal_error("Invalid preferred family.");

  thread_net->callback(requester, [this, requester, hostname, family, preferred, callback = std::move(callback)]() {
      thread_net->udns()->resolve(requester, hostname, family, [this, requester, preferred, callback = std::move(callback)](sin_shared_ptr sin, sin6_shared_ptr sin6, int err) {
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

          m_thread->callback(requester, [result, err, callback = std::move(callback)]() {
                callback(result, err);
            });
        });
    });
}

void
Resolver::resolve_specific(void* requester, const std::string& hostname, int family, single_callback&& callback) {
  thread_net->callback(requester, [this, requester, hostname, family, callback = std::move(callback)]() {
      thread_net->udns()->resolve(requester, hostname, family, [this, requester, family, callback = std::move(callback)](sin_shared_ptr sin, sin6_shared_ptr sin6, int err) {
          sa_shared_ptr result;

          if(err == 0) {
            if (family == AF_INET && sin != nullptr)
              result = sa_copy_in(sin.get());

            if (family == AF_INET6 && sin6 != nullptr)
              result = sa_copy_in6(sin6.get());
          }

          m_thread->callback(requester, [result, err, callback = std::move(callback)]() {
                callback(result, err);
            });
        });
    });
}

void
Resolver::cancel(void* requester) {
  assert(std::this_thread::get_id() == m_thread->thread_id());

  // While processing results, udns is locked so we only need to cancel the callback before
  // canceling the request.
  thread_net->cancel_callback_and_wait(requester);

  torrent::thread_net->udns()->cancel(requester);

  m_thread->cancel_callback_and_wait(requester);
}

} // namespace
