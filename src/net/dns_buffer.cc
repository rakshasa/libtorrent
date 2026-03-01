#include "config.h"

#include "net/dns_buffer.h"

#include <algorithm>
#include <cassert>

#include "net/thread_net.h"
#include "net/udns_resolver.h"
#include "torrent/exceptions.h"

namespace torrent::net {

namespace {
}

DnsBuffer::DnsBuffer() = default;
DnsBuffer::~DnsBuffer() = default;

// We don't try to resolve numeric addresses here, as that should be done in DnsCache or UdnsResolver.

void
DnsBuffer::resolve(void* requester, const std::string& hostname, int family, resolver_callback&& fn) {
  assert(std::this_thread::get_id() == ThreadNet::thread_net()->thread_id());

  auto requester_itr = m_requesters.find(requester);

  if (requester_itr == m_requesters.end())
    requester_itr = m_requesters.emplace(requester, std::make_shared<DnsBufferRequester>()).first;

  auto callback = DnsBufferCallback{requester_itr->second, std::move(fn)};

  // Check for existing active or pending query:

  for (auto& query : m_active_queries) {
    if (query.family == family && query.hostname == hostname) {
      query.callbacks.push_back(std::move(callback));
      requester_itr->second->active_query_count++;
      return;
    }
  }

  for (auto& query : m_pending_queries) {
    if (query.family == family && query.hostname == hostname) {
      query.callbacks.push_back(std::move(callback));
      requester_itr->second->pending_query_count++;
      return;
    }
  }

  // This is a new query:

  auto query = DnsBufferQuery{family, hostname, {}};

  query.callbacks.push_back(std::move(callback));

  if (m_active_queries.size() < max_requests) {
    requester_itr->second->active_query_count++;

    activate_and_resolve_query(std::move(query));

  } else {
    requester_itr->second->pending_query_count++;

    m_pending_queries.insert(m_pending_queries.end(), std::move(query));
  }
}

void
DnsBuffer::cancel(void* requester) {
  assert(std::this_thread::get_id() == ThreadNet::thread_net()->thread_id());

  if (requester == nullptr)
    throw internal_error("DnsBuffer::cancel() called with null requester");

  auto requester_itr = m_requesters.find(requester);

  if (requester_itr == m_requesters.end())
    return;

  m_requesters.erase(requester_itr);
}

void
DnsBuffer::activate_pending_query() {
  while (!m_pending_queries.empty()) {
    if (m_active_query_count >= max_requests)
      throw internal_error("DnsBuffer::activate_pending_query() m_active_query_count >= max_requests");

    auto query = std::move(m_pending_queries.front());
    m_pending_queries.pop_front();

    // Clear expired callbacks. (to these in helper functions)
    query.callbacks.erase(std::remove_if(query.callbacks.begin(), query.callbacks.end(), [](const auto& callback) {
        auto requester = callback.requester.lock();

        if (requester == nullptr)
          return true;

        if (requester->pending_query_count == 0)
          throw internal_error("DnsBuffer::activate_pending_query() requester pending_query_count is already 0");

        requester->pending_query_count--;
        requester->active_query_count++;
        return false;

      }), query.callbacks.end());

    if (query.callbacks.empty())
      continue;

    activate_and_resolve_query(std::move(query));
    return;
  }
}

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

  if (query.family != AF_UNSPEC && query.family != AF_INET && query.family != AF_INET6)
    throw internal_error("DnsBuffer::process() query.family is invalid");

  for (auto& callback : query.callbacks) {
    auto requester = callback.requester.lock();

    if (requester == nullptr)
      continue;

    if (requester->active_query_count == 0)
      throw internal_error("DnsBuffer::process() requester active_query_count is already 0");

    requester->active_query_count--;

    callback.callback(std::move(result_sin), std::move(result_sin6), error);
  }

  activate_pending_query();
}

} // namespace torrent::net
