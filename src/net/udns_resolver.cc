#include "config.h"

#include "net/udns_resolver.h"

#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "globals.h"
#include "manager.h"
#include "net/udns/udns.h"
#include "torrent/common.h"
#include "torrent/poll.h"
#include "torrent/net/socket_address.h"
#include "torrent/utils/log.h"
#include "torrent/utils/thread.h"

#define LT_LOG(log_fmt, ...)                                \
  lt_log_print_subsystem(LOG_NET_DNS, "dns", log_fmt, __VA_ARGS__);

namespace torrent {

bool UdnsResolver::m_initialized = false;

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

UdnsResolver::UdnsResolver() {
  if (!m_initialized) {
    LT_LOG("initializing udns", 0);
    ::dns_init(nullptr, 0);
    m_initialized = true;
  }

  // Contexts are not thread-safe.
  m_ctx = ::dns_new(nullptr);
  m_fileDesc = ::dns_open(m_ctx);

  if (m_fileDesc == -1)
    throw internal_error("dns_init failed");

  m_task_timeout.slot() = [this]() { process_timeouts(); };
}

UdnsResolver::~UdnsResolver() {
  this_thread::scheduler()->erase(&m_task_timeout);

  ::dns_close(m_ctx);
  ::dns_free(m_ctx);

  m_fileDesc = -1;
}

void
UdnsResolver::resolve(void* requester, const std::string& hostname, int family, resolver_callback&& callback) {
  auto query = std::make_unique<Query>();

  query->requester = requester;
  query->hostname = hostname;
  query->family = family;
  query->callback = std::move(callback);
  query->parent = this;

  if (family == AF_INET || family == AF_UNSPEC) {
    query->a4_query = ::dns_submit_a4(m_ctx, hostname.c_str(), 0, a4_callback_wrapper, query.get());

    if (query->a4_query == nullptr) {
      LT_LOG("malformed A query : requester:%p name:%s", requester, hostname.c_str());

      // Unrecoverable errors, like ENOMEM.
      if (::dns_status(m_ctx) != DNS_E_BADQUERY)
        throw new internal_error("dns_submit_a4 failed");

      // UDNS will fail immediately during submission of malformed domain names,
      // e.g., `..`. In order to maintain a clean interface, keep track of this
      // query internally so we can call the callback later with a failure code.
      query->error_sin = EAI_NONAME;

      auto lock = std::scoped_lock(m_mutex);
      m_malformed_queries.insert({requester, std::move(query)});

      return;
    }
  }

  if (family == AF_INET6 || family == AF_UNSPEC) {
    query->a6_query = ::dns_submit_a6(m_ctx, hostname.c_str(), 0, a6_callback_wrapper, query.get());

    // It should be impossible for dns_submit_a6 to fail if dns_submit_a4
    // succeeded, but just in case, make it a hard failure.
    if (query->a6_query == nullptr) {
      LT_LOG("malformed AAAA query : requester:%p name:%s", requester, hostname.c_str());

      if (::dns_status(m_ctx) != DNS_E_BADQUERY)
        throw new internal_error("dns_submit_a6 failed");

      if (query->a4_query != nullptr) {
        ::dns_cancel(m_ctx, query->a4_query);
        query->a4_query = nullptr;
      }

      query->error_sin = EAI_NONAME;

      auto lock = std::scoped_lock(m_mutex);
      m_malformed_queries.insert({requester, std::move(query)});

      return;
    }
  }

  LT_LOG("resolving : requester:%p name:%s family:%d", requester, hostname.c_str(), family);

  auto lock = std::scoped_lock(m_mutex);
  m_queries.insert({requester, std::move(query)});
}

void
UdnsResolver::cancel(void* requester) {
  auto lock = std::scoped_lock(m_mutex);

  auto range = m_queries.equal_range(requester);
  unsigned int query_count = std::distance(range.first, range.second);

  for (auto itr = range.first; itr != range.second; ++itr)
    itr->second->canceled = true;

  range = m_malformed_queries.equal_range(requester);
  unsigned int malformed_count = std::distance(range.first, range.second);

  for (auto itr = range.first; itr != range.second; ++itr)
    itr->second->canceled = true;

  LT_LOG("canceled : requester:%p queries:%d malformed:%d", requester, query_count, malformed_count);
}

void
UdnsResolver::flush() {
  {
    auto lock = std::scoped_lock(m_mutex);
    auto malformed_queries = std::move(m_malformed_queries);

    for (auto& query : malformed_queries) {
      if (query.second->canceled)
        continue;

      LT_LOG("flushing malformed query : requester:%p name:%s", query.first, query.second->hostname.c_str());

      int error = query.second->error_sin != 0 ? query.second->error_sin : query.second->error_sin6;
      if (error == 0)
        throw internal_error("attempting to flush malformed query with no error");

      query.second->callback(nullptr, nullptr, error);
    }
  }

  process_timeouts();
}

void
UdnsResolver::event_read() {
  ::dns_ioevent(m_ctx, 0);
}

void
UdnsResolver::event_write() {
  throw internal_error("UdnsResolver::event_write() called");
}

void
UdnsResolver::event_error() {
  // TODO: Handle error.
}

std::unique_ptr<UdnsResolver::Query>
UdnsResolver::erase_query(query_map::iterator itr) {
  if (itr == m_queries.end())
    throw internal_error("UdnsResolver::erase_query called with invalid iterator");

  auto query = std::move(itr->second);
  m_queries.erase(itr);

  query->deleted = true;
  return query;
}


UdnsResolver::query_map::iterator
UdnsResolver::find_query(Query* query) {
  auto range = m_queries.equal_range(query->requester);

  for (auto itr = range.first; itr != range.second; ++itr) {
    if (itr->second.get() == query)
      return itr;
  }

  return m_queries.end();
}

UdnsResolver::query_map::iterator
UdnsResolver::find_malformed_query(Query* query) {
  auto range = m_malformed_queries.equal_range(query->requester);

  for (auto itr = range.first; itr != range.second; ++itr) {
    if (itr->second.get() == query)
      return itr;
  }

  return m_malformed_queries.end();
}

void
UdnsResolver::process_timeouts() {
  int timeout = ::dns_timeouts(m_ctx, -1, 0);

  if (timeout == -1) {
    thread_self()->poll()->remove_read(this);
    thread_self()->poll()->remove_error(this);
    return;
  }

  thread_self()->poll()->insert_read(this);
  thread_self()->poll()->insert_error(this);

  this_thread::scheduler()->update_for_ceil_seconds(&m_task_timeout, timeout * 1s);
}

void
UdnsResolver::a4_callback_wrapper(struct ::dns_ctx *ctx, ::dns_rr_a4 *result, void *data) {
  auto query = static_cast<UdnsResolver::Query*>(data);

  auto lock = std::scoped_lock(query->parent->m_mutex);

  if (query->deleted) {
    LT_LOG("A records received, but query was deleted : requester:%p name:%s", query->requester, query->hostname.c_str());
    throw internal_error("UdnsResolver::a4_callback_wrapper called with deleted query");
  }

  auto itr = query->parent->find_query(static_cast<UdnsResolver::Query*>(data));
  if (itr == query->parent->m_queries.end())
    throw internal_error("UdnsResolver::a4_callback_wrapper called with invalid query");

  query->a4_query = nullptr;

  if (result == nullptr || result->dnsa4_nrr == 0) {
    query->error_sin = udnserror_to_gaierror(::dns_status(ctx));

    LT_LOG("no A records received : requester:%p name:%s error:'%s'",
           query->requester, query->hostname.c_str(), gai_strerror(query->error_sin));

    process_result(itr);
    return;
  }

  query->result_sin = sin_make();
  query->result_sin->sin_addr = result->dnsa4_addr[0];

  LT_LOG("A records received : requester:%p name:%s nrr:%d",
         query->requester, query->hostname.c_str(), result->dnsa4_nrr);

  process_result(itr);
}

void
UdnsResolver::a6_callback_wrapper(struct ::dns_ctx *ctx, ::dns_rr_a6 *result, void *data) {
  auto query = static_cast<UdnsResolver::Query*>(data);

  auto lock = std::scoped_lock(query->parent->m_mutex);

  if (query->deleted) {
    LT_LOG("AAAA records received, but query was deleted : requester:%p name:%s", query->requester, query->hostname.c_str());
    throw internal_error("UdnsResolver::a6_callback_wrapper called with deleted query");
  }

  auto itr = query->parent->find_query(query);
  if (itr == query->parent->m_queries.end())
    throw internal_error("UdnsResolver::a6_callback_wrapper called with invalid query");

  query->a6_query = nullptr;

  if (result == nullptr || result->dnsa6_nrr == 0) {
    query->error_sin6 = udnserror_to_gaierror(::dns_status(ctx));

    LT_LOG("no AAAA records received, calling back with error : requester:%p name:%s error:'%s'",
           query->requester, query->hostname.c_str(), gai_strerror(query->error_sin6));

    process_result(itr);
    return;
  }

  query->result_sin6 = sin6_make();
  query->result_sin6->sin6_addr = result->dnsa6_addr[0];

  LT_LOG("AAAA records received : requester:%p name:%s nrr:%d",
         query->requester, query->hostname.c_str(), result->dnsa6_nrr);

  process_result(itr);
}

void
UdnsResolver::process_result(query_map::iterator itr) {
  if (itr->second->a4_query != nullptr || itr->second->a6_query != nullptr)
    return;

  auto query = itr->second->parent->erase_query(itr);

  if (query->canceled) {
    LT_LOG("processing results, canceled : requester:%p name:%s", query->requester, query->hostname.c_str());
    return;
  }

  LT_LOG("processing results, calling back : requester:%p name:%s", query->requester, query->hostname.c_str());

  if (query->result_sin == nullptr && query->result_sin6 == nullptr) {
    query->callback(nullptr, nullptr, query->error_sin != 0 ? query->error_sin : query->error_sin6);
    return;
  }

  query->callback(query->result_sin, query->result_sin6, 0);
}

} // namespace torrent
