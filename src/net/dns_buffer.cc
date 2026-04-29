#include "config.h"

#include "net/dns_buffer.h"

#include <algorithm>
#include <cassert>
#include <netdb.h>

#include "net/dns_cache.h"
#include "net/thread_net.h"
#include "net/udns_resolver.h"
#include "torrent/exceptions.h"
#include "torrent/net/socket_address.h"
#include "torrent/utils/log.h"

#define LT_LOG(log_fmt, ...)                                            \
  lt_log_print_subsystem(LOG_NET_DNS, "dns-buffer", log_fmt, __VA_ARGS__);
#define LT_LOG_REQUESTER(log_fmt, ...)                                  \
  lt_log_print(LOG_NET_DNS, "%016p->dns-buffer : " log_fmt, requester, __VA_ARGS__);

namespace torrent::net {

DnsBuffer::~DnsBuffer() {
  // assert(std::this_thread::get_id() == ThreadNet::thread_net()->thread_id());

  // if (m_active_query_count != 0)
  //   throw internal_error("DnsBuffer::~DnsBuffer() m_active_query_count != 0");

  // if (!m_pending_queries.empty())
  //   throw internal_error("DnsBuffer::~DnsBuffer() !m_pending_queries.empty()");

  {
    // Lock to ensure cancel_safe() calls are not in progress.
    auto guard = std::lock_guard(m_requesters_mutex);

    m_requesters.clear();
  }
}

// Numeric addresses are resolved in torrent/net/resolver, with no use of ThreadNet.

void
DnsBuffer::resolve(void* requester, const std::string& hostname, int family, resolver_callback&& fn) {
  assert(std::this_thread::get_id() == ThreadNet::thread_net()->thread_id());

  auto initialize_fn = [this, requester, fn = std::move(fn)](bool is_active) mutable {
      auto guard = std::lock_guard(m_requesters_mutex);
      auto itr   = m_requesters.find(requester);

      if (itr == m_requesters.end())
        itr = m_requesters.emplace(requester, std::make_shared<DnsBufferRequester>()).first;

      // TODO: Add sanity check for count during development.

      if (is_active)
        itr->second->active_query_count++;
      else
        itr->second->pending_query_count++;

      return DnsBufferCallback{itr->second, std::move(fn)};
    };

  // Check for existing active or pending query:

  if (m_active_query_count != 0) {
    for (auto& query : m_active_queries) {
      if (query.family == family && query.hostname == hostname) {
        query.callbacks.push_back(initialize_fn(true));

        LT_LOG_REQUESTER("added to active query : name:%s family:%d", hostname.c_str(), family);
        return;
      }
    }

    for (auto& query : m_pending_queries) {
      if (query.family == family && query.hostname == hostname) {
        query.callbacks.push_back(initialize_fn(false));

        LT_LOG_REQUESTER("added to pending query : name:%s family:%d", hostname.c_str(), family);
        return;
      }
    }
  }

  // No existing query, create a new one:

  if (m_active_query_count < max_requests) {
    LT_LOG_REQUESTER("added new active query : name:%s family:%d", hostname.c_str(), family);

    activate_and_resolve_query(DnsBufferQuery{family, hostname, {initialize_fn(true)}});
    return;
  }

  LT_LOG_REQUESTER("added new pending query : name:%s family:%d", hostname.c_str(), family);

  m_pending_queries.insert(m_pending_queries.end(), DnsBufferQuery{family, hostname, {initialize_fn(false)}});
}

// TODO: We're not flushing old m_requesters. (do we make calling cancel_safe() required for all requesters?)

void
DnsBuffer::cancel_safe(void* requester) {
  if (requester == nullptr)
    throw internal_error("DnsBuffer::cancel() called with null requester");

  {
    auto guard = std::lock_guard(m_requesters_mutex);
    auto itr   = m_requesters.find(requester);

    if (itr == m_requesters.end())
      return;

    m_requesters.erase(itr);
  }

  LT_LOG_REQUESTER("canceled", 0);
}

void
DnsBuffer::activate_pending_query() {
  while (!m_pending_queries.empty()) {
    if (m_active_query_count >= max_requests)
      throw internal_error("DnsBuffer::activate_pending_query() m_active_query_count >= max_requests");

    auto query = std::move(m_pending_queries.front());
    m_pending_queries.pop_front();

    auto erase_itr = std::remove_if(query.callbacks.begin(), query.callbacks.end(), [](const auto& callback) {
        auto ptr       = callback.requester.lock();
        auto requester = ptr.get();

        if (requester == nullptr)
          return true;

        if (requester->pending_query_count == 0)
          throw internal_error("DnsBuffer::activate_pending_query() requester pending_query_count is already 0");

        requester->pending_query_count--;
        requester->active_query_count++;

        LT_LOG_REQUESTER("moved from pending to active query", 0);

        return false;
      });

    query.callbacks.erase(erase_itr, query.callbacks.end());

    if (query.callbacks.empty()) {
      LT_LOG("skipping pending query with no callbacks : name:%s family:%d", query.hostname.c_str(), query.family);
      continue;
    }

    activate_and_resolve_query(std::move(query));
    return;
  }
}

void
DnsBuffer::activate_and_resolve_query(DnsBufferQuery query) {
  assert(std::this_thread::get_id() == ThreadNet::thread_net()->thread_id());

  if (m_active_query_count >= max_requests)
    throw internal_error("DnsBuffer::activate_query() m_active_query_count >= max_requests");

  if (query.callbacks.empty())
    throw internal_error("DnsBuffer::activate_query() called with empty callbacks");

  auto itr = std::find_if(m_active_queries.begin(), m_active_queries.end(), [](const auto& query) {
      return query.family == -1;
    });

  if (itr == m_active_queries.end())
    throw internal_error("DnsBuffer::activate_query() itr == m_active_queries.end()");

  *itr = std::move(query);
  m_active_query_count++;

  auto index = std::distance(m_active_queries.begin(), itr);

  auto fn = [this, index](sin_shared_ptr result_sin, int error_sin, sin6_shared_ptr result_sin6, int error_sin6) {
      this_thread::callback(this->requester_from_index(index), [=]() {
          this->process(index, std::move(result_sin), error_sin, std::move(result_sin6), error_sin6);
        });
    };

  // LT_LOG("activating query : requesters:%zu name:%s family:%d index:%u",
  //        itr->callbacks.size(), itr->hostname.c_str(), itr->family, index);

  ThreadNet::thread_net()->dns_resolver()->resolve(requester_from_index(index), itr->hostname, itr->family, std::move(fn));
}

void
DnsBuffer::process(unsigned int index, sin_shared_ptr result_sin, int error_sin, sin6_shared_ptr result_sin6, int error_sin6) {
  assert(std::this_thread::get_id() == ThreadNet::thread_net()->thread_id());

  if (index >= max_requests)
    throw internal_error("DnsBuffer::process() index out of bounds");

  auto query = std::move(m_active_queries[index]);

  m_active_queries[index] = DnsBufferQuery{};
  m_active_query_count--;

  if (query.family != AF_UNSPEC && query.family != AF_INET && query.family != AF_INET6)
    throw internal_error("DnsBuffer::process() query.family is invalid");

  if (result_sin && error_sin != 0)
    throw internal_error("DnsBuffer::process() query has both result and error for A record");

  if (result_sin6 && error_sin6 != 0)
    throw internal_error("DnsBuffer::process() query has both result and error for AAAA record");

  if (query.family == AF_UNSPEC) {
    if (result_sin == nullptr && result_sin6 == nullptr && (error_sin == 0 || error_sin6 == 0))
      throw internal_error("DnsBuffer::process() query has no result and no error for AF_UNSPEC");

  } else if (query.family == AF_INET) {
    if (result_sin == nullptr && error_sin == 0)
      throw internal_error("DnsBuffer::process() query has no result and no error for AF_INET");
    if (result_sin6 != nullptr || error_sin6 != 0)
      throw internal_error("DnsBuffer::process() query has result or error for AF_INET6 when family is AF_INET");

  } else if (query.family == AF_INET6) {
    if (result_sin6 == nullptr && error_sin6 == 0)
      throw internal_error("DnsBuffer::process() query has no result and no error for AF_INET6");
    if (result_sin != nullptr || error_sin != 0)
      throw internal_error("DnsBuffer::process() query has result or error for AF_INET when family is AF_INET6");
  }

  if (result_sin)
    ThreadNet::thread_net()->dns_cache()->process_success(query.hostname, AF_INET, result_sin, nullptr);
  else if (error_sin != 0)
    ThreadNet::thread_net()->dns_cache()->process_failure(query.hostname, AF_INET, error_sin);

  if (result_sin6)
    ThreadNet::thread_net()->dns_cache()->process_success(query.hostname, AF_INET6, nullptr, result_sin6);
  else if (error_sin6 != 0)
    ThreadNet::thread_net()->dns_cache()->process_failure(query.hostname, AF_INET6, error_sin6);

  for (auto& callback : query.callbacks)
    process_callback(callback, result_sin, error_sin, result_sin6, error_sin6);

  activate_pending_query();
}

void
DnsBuffer::process_callback(DnsBufferCallback& callback, sin_shared_ptr result_sin, int error_sin, sin6_shared_ptr result_sin6, int error_sin6) {
  auto requester_ptr = callback.requester.lock();
  auto requester     = requester_ptr.get();

  if (requester == nullptr)
    return;

  if (requester->active_query_count == 0)
    throw internal_error("DnsBuffer::process() requester active_query_count is already 0");

  requester->active_query_count--;

  LT_LOG_REQUESTER("processing callback : inet:%s inet6:%s",
                   (error_sin == 0) ? sin_pretty_or_empty(result_sin.get()).c_str() : gai_enum_error(error_sin),
                   (error_sin6 == 0) ? sin6_pretty_or_empty(result_sin6.get()).c_str() : gai_enum_error(error_sin6));

  {
    // Block cancel() until this is done to ensure callbacks for the requester are all canceled.
    auto guard = std::lock_guard(m_requesters_mutex);

    if (requester_ptr.use_count() > 1)
      callback.callback(result_sin, error_sin, result_sin6, error_sin6);
  }
}

void*
DnsBuffer::requester_from_index(unsigned int index) {
  if (index >= max_requests)
    throw internal_error("DnsBuffer::requester_from_index() index out of bounds");

  return m_active_queries.data() + index;
}

} // namespace torrent::net
