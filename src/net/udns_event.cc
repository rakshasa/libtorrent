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
#include "torrent/utils/log.h"
#include "torrent/utils/thread.h"

#define LT_LOG(log_fmt, ...)                                \
  lt_log_print_subsystem(LOG_NET_DNS, "dns", log_fmt, __VA_ARGS__);

// Assertion failed: (nactive == ctx->dnsc_nactive), function dns_assert_ctx, file udns_resolver.c, line 221.

// Caught internal_error: priority_queue_insert(...) called on an already queued item.
// 0   libtorrent.23.dylib                 0x0000000106a6bc9f _ZN7torrent14internal_error10initializeERKNSt3__112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEE + 63
// 1   libtorrent.23.dylib                 0x0000000106a2f0e8 _ZN7torrent14internal_errorC2EPKc + 184
// 2   libtorrent.23.dylib                 0x0000000106a2e3c2 _ZN3rak21priority_queue_insertEPNS_14priority_queueIPNS_13priority_itemENS_16priority_compareENSt3__18equal_toIS2_EEEES2_NS_5timerE + 178
// 3   libtorrent.23.dylib                 0x0000000106ad24bb _ZN7torrent10TrackerUdp14start_announceEPK8sockaddri + 1099
// 4   libtorrent.23.dylib                 0x0000000106a46bcd _ZNSt3__110__function6__funcIZN7torrent3net8Resolver16process_callbackEPvPK8sockaddriONS_8functionIFvS8_iEEEE3$_0NS_9allocatorISD_EEFvvEEclEv + 45
// 5   libtorrent.23.dylib                 0x0000000106a66a5b _ZN7torrent5utils6Thread17process_callbacksEv + 251
// 6   libtorrent.23.dylib                 0x0000000106a665db _ZN7torrent5utils6Thread10event_loopEPS1_ + 283
// 7   rtorrent                            0x0000000106361424 main + 11348
// 8   dyld                                0x00007ff8003492cd start + 1805

// Trackers request the same number of times their group #, with no delays.

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

    // TODO: LOG initialization.
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
UdnsEvent::resolve(void* requester, const char *name, int family, resolver_callback&& callback) {
  auto query = std::unique_ptr<Query>(new Query{
      requester,
      name,
      family,
      this,
      false,
      nullptr,
      nullptr,
      callback,
      0});

  if (family == AF_INET || family == AF_UNSPEC) {
    query->a4_query = ::dns_submit_a4(m_ctx, name, 0, a4_callback_wrapper, query.get());

    if (query->a4_query == nullptr) {
      LT_LOG("malformed A query : requester:%p name:'%s'", requester, name);

      // Unrecoverable errors, like ENOMEM.
      if (::dns_status(m_ctx) != DNS_E_BADQUERY)
        throw new internal_error("dns_submit_a4 failed");

      // UDNS will fail immediately during submission of malformed domain names,
      // e.g., `..`. In order to maintain a clean interface, keep track of this
      // query internally so we can call the callback later with a failure code.
      query->error = EAI_NONAME;

      std::lock_guard<std::mutex> lock(m_mutex);
      m_malformed_queries.insert({requester, std::move(query)});

      return;
    }
  }

  if (family == AF_INET6 || family == AF_UNSPEC) {
    query->a6_query = ::dns_submit_a6(m_ctx, name, 0, a6_callback_wrapper, query.get());

    // It should be impossible for dns_submit_a6 to fail if dns_submit_a4
    // succeeded, but just in case, make it a hard failure.
    if (query->a6_query == nullptr) {
      LT_LOG("malformed AAAA query : requester:%p name:'%s'", requester, name);

      if (::dns_status(m_ctx) != DNS_E_BADQUERY)
        throw new internal_error("dns_submit_a6 failed");

      query->error = EAI_NONAME;

      std::lock_guard<std::mutex> lock(m_mutex);
      m_malformed_queries.insert({requester, std::move(query)});

      return;
    }
  }

  LT_LOG("resolving : requester:%p name:'%s' family:%d", requester, name, family);

  std::lock_guard<std::mutex> lock(m_mutex);
  m_queries.insert({requester, std::move(query)});
}

void
UdnsEvent::cancel(void* requester) {
  std::lock_guard<std::mutex> lock(m_mutex);

  auto range = m_queries.equal_range(requester);
  unsigned int query_count = std::distance(range.first, range.second);

  for (auto itr = range.first; itr != range.second; ++itr)
    itr->second->cancelled = true;

  range = m_malformed_queries.equal_range(requester);
  unsigned int malformed_count = std::distance(range.first, range.second);

  for (auto itr = range.first; itr != range.second; ++itr)
    itr->second->cancelled = true;

  LT_LOG("cancelled : requester:%p queries:%d malformed:%d", requester, query_count, malformed_count);
}

void
UdnsEvent::flush() {
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto malformed_queries = std::move(m_malformed_queries);

    for (auto& query : malformed_queries) {
      if (!query.second->cancelled) {
        LT_LOG("flushing malformed query : requester:%p name:'%s'", query.first, query.second->name.c_str());
        query.second->callback(nullptr, query.second->error);
      }
    }
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

UdnsEvent::query_map::iterator
UdnsEvent::find_query(Query* query) {
  auto range = m_queries.equal_range(query->requester);

  for (auto itr = range.first; itr != range.second; ++itr) {
    if (itr->second.get() == query)
      return itr;
  }

  return m_queries.end();
}

UdnsEvent::query_map::iterator
UdnsEvent::find_malformed_query(Query* query) {
  auto range = m_malformed_queries.equal_range(query->requester);

  for (auto itr = range.first; itr != range.second; ++itr) {
    if (itr->second.get() == query)
      return itr;
  }

  return m_malformed_queries.end();
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
  auto query = static_cast<UdnsEvent::Query*>(data);

  std::lock_guard<std::mutex> lock(query->parent->m_mutex);

  auto itr = query->parent->find_query(query);
  if (itr == query->parent->m_queries.end())
    throw internal_error("UdnsEvent::a4_callback_wrapper called with invalid query");

  query->a4_query = nullptr;

  if (result == nullptr || result->dnsa4_nrr == 0) {
    if (query->a6_query != nullptr) {
      LT_LOG("no A records, waiting for AAAA : requester:%p name:'%s'", query->requester, query->name.c_str());
      return;
    }

    auto callback = query->callback;
    query->parent->m_queries.erase(itr);

    if (query->cancelled) {
      LT_LOG("no A records, cancelled : requester:%p name:'%s'", query->requester, query->name.c_str());
      return;
    }

    LT_LOG("no A records, calling back with error : requester:%p name:'%s' error:'%s'",
           query->requester, query->name.c_str(), gai_strerror(udnserror_to_gaierror(::dns_status(ctx))));

    callback(nullptr, udnserror_to_gaierror(::dns_status(ctx)));
    return;
  }

  // TODO: Change this to wait for a6 response before the callback.
  if (query->a6_query != nullptr) {
    LT_LOG("A records received, skipping AAAA : requester:%p name:'%s'", query->requester, query->name.c_str());
    ::dns_cancel(ctx, query->a6_query);
  }

  struct sockaddr_in sa{};
  sa.sin_family = AF_INET;
  sa.sin_port = 0;
  sa.sin_addr = result->dnsa4_addr[0];

  auto callback = query->callback;
  query->parent->m_queries.erase(itr);

  if (query->cancelled) {
    LT_LOG("A records received, cancelled : requester:%p name:'%s' nrr:%d",
           query->requester, query->name.c_str(), result->dnsa4_nrr);
    return;
  }

  LT_LOG("A records received, calling back : requester:%p name:'%s' nrr:%d",
         query->requester, query->name.c_str(), result->dnsa4_nrr);

  callback(reinterpret_cast<sockaddr*>(&sa), 0);
}

void
UdnsEvent::a6_callback_wrapper(struct ::dns_ctx *ctx, ::dns_rr_a6 *result, void *data) {
  auto query = static_cast<UdnsEvent::Query*>(data);
  query->a6_query = nullptr;

  std::lock_guard<std::mutex> lock(query->parent->m_mutex);

  auto itr = query->parent->find_query(query);
  if (itr == query->parent->m_queries.end())
    throw internal_error("UdnsEvent::a6_callback_wrapper called with invalid query");

  if (result == nullptr || result->dnsa6_nrr == 0) {
    if (query->a4_query != nullptr) {
      LT_LOG("no AAAA records, waiting for A : requester:%p name:'%s'", query->requester, query->name.c_str());
      return;
    }

    auto callback = query->callback;
    query->parent->m_queries.erase(itr);

    if (query->cancelled) {
      LT_LOG("no AAAA records, cancelled : requester:%p name:'%s'", query->requester, query->name.c_str());
      return;
    }

    LT_LOG("no AAAA records, calling back with error : requester:%p name:'%s' error:'%s'",
           query->requester, query->name.c_str(), gai_strerror(udnserror_to_gaierror(::dns_status(ctx))));

    callback(nullptr, udnserror_to_gaierror(::dns_status(ctx)));
    return;
  }

  if (query->a4_query != nullptr) {
    LT_LOG("AAAA records received, skipping A : requester:%p name:'%s'", query->requester, query->name.c_str());
    ::dns_cancel(ctx, query->a4_query);
  }

  struct sockaddr_in6 sa{};
  sa.sin6_family = AF_INET6;
  sa.sin6_port = 0;
  sa.sin6_addr = result->dnsa6_addr[0];

  auto callback = query->callback;
  query->parent->m_queries.erase(itr);

  if (query->cancelled) {
    LT_LOG("AAAA records received, cancelled : requester:%p name:'%s' nrr:%d",
           query->requester, query->name.c_str(), result->dnsa6_nrr);
    return;
  }

  LT_LOG("AAAA records received, calling back : requester:%p name:'%s' nrr:%d",
         query->requester, query->name.c_str(), result->dnsa6_nrr);

  callback(reinterpret_cast<sockaddr*>(&sa), 0);
}

} // namespace torrent
