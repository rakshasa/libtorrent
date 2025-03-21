#include "config.h"

#include "torrent/net/resolver.h"

#include "net/thread_net.h"
#include "net/udns_event.h"
#include "torrent/utils/thread.h"

namespace torrent::net {

void
Resolver::init() {
  m_thread = torrent::thread_self;
}

void
Resolver::resolve(void* requester, const char* host, int family, resolver_callback&& slot) {
  thread_net->callback(requester, [this, requester, host, family, slot = std::move(slot)]() {
      thread_net->udns()->resolve(requester, host, family, [this, requester, slot = std::move(slot)](const sockaddr* addr, int err) {
          this->process_callback(requester, addr, err, (resolver_callback)slot);
        });
    });
}

void
Resolver::cancel(void* requester) {
  torrent::thread_net->udns()->cancel(requester);

  m_thread->cancel_callback(requester);
  thread_net->cancel_callback(requester);
}

void
Resolver::process_callback(void *requester, const sockaddr* addr, int err, resolver_callback&& slot) {
  m_thread->callback(requester, [addr, err, slot = std::move(slot)]() {
      slot(addr, err);
    });
}

} // namespace
