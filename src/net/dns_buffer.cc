#include "config.h"

#include "net/dns_buffer.h"

#include <algorithm>
#include <cassert>
#include <netdb.h>

#include "net/thread_net.h"
#include "net/udns_resolver.h"
#include "torrent/exceptions.h"
#include "torrent/net/socket_address.h"
#include "torrent/utils/log.h"

#define LT_LOG(log_fmt, ...)                                \
  lt_log_print_subsystem(LOG_NET_DNS, "dns-buffer", log_fmt, __VA_ARGS__);

namespace torrent::net {

namespace {
}

DnsBuffer::DnsBuffer() = default;
DnsBuffer::~DnsBuffer() = default;

// We don't try to resolve numeric addresses here, as that should be done in DnsCache or UdnsResolver.

void
DnsBuffer::resolve(void* requester, const std::string& hostname, int family, resolver_callback&& fn) {
  assert(std::this_thread::get_id() == ThreadNet::thread_net()->thread_id());

  // TODO: Lock for whole function, or to activate_and_resolve_query() (or to increment counts)
  // TODO: We don't need to synchrnoize the requester objects, they don't have dtors.

  auto requester_itr = m_requesters.find(requester);

  if (requester_itr == m_requesters.end())
    requester_itr = m_requesters.emplace(requester, std::make_shared<DnsBufferRequester>()).first;

  auto callback = DnsBufferCallback{requester_itr->second, std::move(fn)};

  // Check for existing active or pending query:

  if (m_active_query_count != 0) {
    for (auto& query : m_active_queries) {
      if (query.family == family && query.hostname == hostname) {
        requester_itr->second->active_query_count++;
        query.callbacks.push_back(std::move(callback));

        // TODO: Add sanity check for count during development.

        LT_LOG("added to active query : requester:%p name:%s family:%d", requester, hostname.c_str(), family);
        return;
      }
    }

    for (auto& query : m_pending_queries) {
      if (query.family == family && query.hostname == hostname) {
        requester_itr->second->pending_query_count++;
        query.callbacks.push_back(std::move(callback));

        // TODO: Add sanity check for count during development.

        LT_LOG("added to pending query : requester:%p name:%s family:%d", requester, hostname.c_str(), family);
        return;
      }
    }
  }

  // No existing query, create a new one:
  auto query = DnsBufferQuery{family, hostname, {std::move(callback)}};

  if (m_active_query_count < max_requests) {
    requester_itr->second->active_query_count++;
    activate_and_resolve_query(std::move(query));

    LT_LOG("added new active query : requester:%p name:%s family:%d", requester, hostname.c_str(), family);
    return;
  }

  requester_itr->second->pending_query_count++;
  m_pending_queries.insert(m_pending_queries.end(), std::move(query));

  LT_LOG("added new pending query : requester:%p name:%s family:%d", requester, hostname.c_str(), family);
}

void
DnsBuffer::cancel(void* requester) {
  assert(std::this_thread::get_id() == ThreadNet::thread_net()->thread_id());

  if (requester == nullptr)
    throw internal_error("DnsBuffer::cancel() called with null requester");

  // TODO: Lock m_requesters.

  auto requester_itr = m_requesters.find(requester);

  if (requester_itr == m_requesters.end())
    return;

  m_requesters.erase(requester_itr);

  LT_LOG("canceled : requester:%p", requester);
}

void
DnsBuffer::activate_pending_query() {
  while (!m_pending_queries.empty()) {
    if (m_active_query_count >= max_requests)
      throw internal_error("DnsBuffer::activate_pending_query() m_active_query_count >= max_requests");

    auto query = std::move(m_pending_queries.front());
    m_pending_queries.pop_front();

    auto erase_itr = std::remove_if(query.callbacks.begin(), query.callbacks.end(), [](const auto& callback) {
        auto requester = callback.requester.lock();

        if (requester == nullptr)
          return true;

        if (requester->pending_query_count == 0)
          throw internal_error("DnsBuffer::activate_pending_query() requester pending_query_count is already 0");

        requester->pending_query_count--;
        requester->active_query_count++;

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

// TODO: Remove return value.
unsigned int
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

  auto fn = [this, index](sin_shared_ptr result_sin, sin6_shared_ptr result_sin6, int error) {
      process(index, std::move(result_sin), std::move(result_sin6), error);
    };

  LT_LOG("activating query : requesters:%zu name:%s family:%d index:%u",
         itr->callbacks.size(), itr->hostname.c_str(), itr->family, index);

  ThreadNet::thread_net()->dns_resolver()->resolve(requester_from_index(index), itr->hostname, itr->family, std::move(fn));

  return index;
}

void
DnsBuffer::process(unsigned int index, sin_shared_ptr result_sin, sin6_shared_ptr result_sin6, int error) {
  assert(std::this_thread::get_id() == ThreadNet::thread_net()->thread_id());

  if (index >= max_requests)
    throw internal_error("DnsBuffer::process() index out of bounds");

  auto query = std::move(m_active_queries[index]);

  m_active_queries[index] = DnsBufferQuery{};
  m_active_query_count--;

  if (error == 0) {
    LT_LOG("processing result : requesters:%zu name:%s family:%d index:%u inet:%s inet6:%s",
           query.callbacks.size(), query.hostname.c_str(), query.family, index,
           sin_pretty_or_empty(result_sin.get()).c_str(), sin6_pretty_or_empty(result_sin6.get()).c_str());
  } else {
    LT_LOG("processing result : requesters:%zu name:%s family:%d index:%u : %s",
           query.callbacks.size(), query.hostname.c_str(), query.family, index, gai_strerror(error));
  }

  if (query.family != AF_UNSPEC && query.family != AF_INET && query.family != AF_INET6)
    throw internal_error("DnsBuffer::process() query.family is invalid");

  for (auto& callback : query.callbacks) {
    auto requester = callback.requester.lock();

    if (requester == nullptr)
      continue;

    if (requester->active_query_count == 0)
      throw internal_error("DnsBuffer::process() requester active_query_count is already 0");

    requester->active_query_count--;

    LT_LOG("processing callback : requester:%p", requester.get());

    // TODO: Lock here, and check if use_count is greater than 1 before calling callback.

    // Block cancel() until this is done to ensure callbacks for the requester are all canceled.
    if (requester.use_count() > 1)
      callback.callback(std::move(result_sin), std::move(result_sin6), error);
  }

  activate_pending_query();
}

void*
DnsBuffer::requester_from_index(unsigned int index) {
  if (index >= max_requests)
    throw internal_error("DnsBuffer::requester_from_index() index out of bounds");

  return m_active_queries.data() + index;
}

} // namespace torrent::net
