#include "config.h"

#include "net/udns_event.h"

#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "globals.h"
#include "manager.h"
#include "net/udns/udns.h"
#include "torrent/common.h"
#include "torrent/poll.h"
#include "torrent/utils/thread.h"

namespace torrent {

bool UdnsEvent::m_initialized = false;

static int
udnserror_to_gaierror(int udnserror) {
  switch (udnserror) {
    case DNS_E_TEMPFAIL:
      return EAI_AGAIN;
    case DNS_E_PROTOCOL:
      // this isn't quite right
      return EAI_FAIL;
    case DNS_E_NXDOMAIN:
      return EAI_NONAME;
    case DNS_E_NODATA:
      return EAI_ADDRFAMILY;
    case DNS_E_NOMEM:
      return EAI_MEMORY;
    case DNS_E_BADQUERY:
      return EAI_NONAME;
    default:
      return EAI_ADDRFAMILY;
  }
}

UdnsEvent::UdnsEvent() {
  if (!m_initialized) {
    ::dns_init(nullptr, 0);
    m_initialized = true;
  }

  // Contexts are not thread-safe.
  m_ctx = ::dns_new(nullptr);
  m_fileDesc = ::dns_open(m_ctx);

  if (m_fileDesc == -1)
    throw internal_error("dns_init failed");

  m_taskTimeout.slot() = [this]() { process_timeouts(); };
}

UdnsEvent::~UdnsEvent() {
  priority_queue_erase(&taskScheduler, &m_taskTimeout);

  // TODO: Do we need to clean up the queries?

  ::dns_close(m_ctx);
  ::dns_free(m_ctx);

  m_fileDesc = -1;
}

void
UdnsEvent::enqueue_resolve(void* requester, const char *name, int family, resolver_callback&& callback) {
  // TODO: Verify correct thread.

  if (m_queries.find(requester) != m_queries.end())
    throw internal_error("requester already exists in UdnsEvent::m_queries");

  if (m_malformed_queries.find(requester) != m_malformed_queries.end())
    throw internal_error("requester already exists in UdnsEvent::m_malformed_queries");

  auto query = std::unique_ptr<Query>(new Query{requester, this, nullptr, nullptr, callback, 0});

  if (family == AF_INET || family == AF_UNSPEC) {
    query->a4_query = ::dns_submit_a4(m_ctx, name, 0, a4_callback_wrapper, query.get());

    if (query->a4_query == nullptr) {
      // Unrecoverable errors, like ENOMEM.
      if (::dns_status(m_ctx) != DNS_E_BADQUERY)
        throw new internal_error("dns_submit_a4 failed");

      // UDNS will fail immediately during submission of malformed domain names,
      // e.g., `..`. In order to maintain a clean interface, keep track of this
      // query internally so we can call the callback later with a failure code.
      query->error = EAI_NONAME;

      m_malformed_queries[requester] = std::move(query);
      return;
    }
  }

  if (family == AF_INET6 || family == AF_UNSPEC) {
    query->a6_query = ::dns_submit_a6(m_ctx, name, 0, a6_callback_wrapper, query.get());

    if (query->a6_query == nullptr) {
      // It should be impossible for dns_submit_a6 to fail if dns_submit_a4
      // succeeded, but just in case, make it a hard failure.

      if (::dns_status(m_ctx) != DNS_E_BADQUERY)
        throw new internal_error("dns_submit_a6 failed");

      query->error = EAI_NONAME;

      m_malformed_queries[requester] = std::move(query);
      return;
    }
  }

  m_queries[requester] = std::move(query);
}

void
UdnsEvent::cancel(void* requester) {
  auto fn = [this, requester](auto& query) {
      if (query.second->requester != requester)
        return false;

      if (query.second->a4_query != nullptr)
        ::dns_cancel(m_ctx, query.second->a4_query);

      if (query.second->a6_query != nullptr)
        ::dns_cancel(m_ctx, query.second->a6_query);

      return true;
    };

  auto itr = m_queries.find(requester);
  if (itr != m_queries.end()) {
    fn(*itr);
    m_queries.erase(itr);
  }

  itr = m_malformed_queries.find(requester);
  if (itr != m_malformed_queries.end()) {
    fn(*itr);
    m_malformed_queries.erase(itr);
  }
}

void
UdnsEvent::flush() {
  while (!m_malformed_queries.empty()) {
    auto itr = m_malformed_queries.begin();
    auto query = std::move(itr->second);

    m_malformed_queries.erase(itr);
    query->callback(nullptr, query->error);
  }

  process_timeouts();
}

void
UdnsEvent::event_read() {
  ::dns_ioevent(m_ctx, 0);
}

void
UdnsEvent::event_write() {
  throw internal_error("UdnsEvent::event_write() called");
}

void
UdnsEvent::event_error() {
  // TODO: Handle error.
}

void
UdnsEvent::process_timeouts() {
  int timeout = ::dns_timeouts(m_ctx, -1, 0);

  if (timeout == -1) {
    thread_self->poll()->remove_read(this);
    thread_self->poll()->remove_error(this);
    return;
  }

  thread_self->poll()->insert_read(this);
  thread_self->poll()->insert_error(this);

  priority_queue_update(&taskScheduler, &m_taskTimeout, (cachedTime + rak::timer::from_seconds(timeout)).round_seconds());
}

void
UdnsEvent::a4_callback_wrapper(struct ::dns_ctx *ctx, ::dns_rr_a4 *result, void *data) {
  struct sockaddr_in sa;
  auto query = static_cast<UdnsEvent::Query*>(data);

  query->a4_query = nullptr;

  if (result == nullptr || result->dnsa4_nrr == 0) {
    if (query->a6_query == nullptr) {
      auto callback = query->callback;

      query->parent->m_queries.erase(query->requester);
      query->callback(nullptr, udnserror_to_gaierror(::dns_status(ctx)));
    }

    return;
  }

  sa.sin_family = AF_INET;
  sa.sin_port = 0;
  sa.sin_addr = result->dnsa4_addr[0];

  // TODO: Change this to wait for a6 response before the callback.
  if (query->a6_query != nullptr) {
    ::dns_cancel(ctx, query->a6_query);
  }

  auto callback = query->callback;

  query->parent->m_queries.erase(query->requester);
  query->callback(reinterpret_cast<sockaddr*>(&sa), 0);
}

void
UdnsEvent::a6_callback_wrapper(struct ::dns_ctx *ctx, ::dns_rr_a6 *result, void *data) {
  struct sockaddr_in6 sa;
  auto query = static_cast<UdnsEvent::Query*>(data);

  query->a6_query = nullptr;

  if (result == nullptr || result->dnsa6_nrr == 0) {
    if (query->a4_query == nullptr) {
      auto callback = query->callback;

      query->parent->m_queries.erase(query->requester);
      query->callback(nullptr, udnserror_to_gaierror(::dns_status(ctx)));
    }

    return;
  }

  sa.sin6_family = AF_INET6;
  sa.sin6_port = 0;
  sa.sin6_addr = result->dnsa6_addr[0];

  if (query->a4_query != nullptr) {
    ::dns_cancel(ctx, query->a4_query);
  }

  auto callback = query->callback;

  query->parent->m_queries.erase(query->requester);
  query->callback(reinterpret_cast<sockaddr*>(&sa), 0);
}

} // namespace torrent
