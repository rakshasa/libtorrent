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
DnsBuffer::resolve(void* requester, const std::string& hostname, int family, resolver_callback&& callback) {
  assert(std::this_thread::get_id() == ThreadNet::thread_net()->thread_id());

  // TODO: First search active and pending queries for a matching hostname and family.

  if (m_active_queries.size() < max_requests) {
    activate_query(DnsBufferQuery{requester, hostname, family, std::move(callback)});
    return;
  }

  m_pending_queries.emplace_back(DnsBufferQuery{requester, hostname, family, std::move(callback)});

  m_requesters[requester].query_iters.push_back(std::prev(m_pending_queries.end()));
}

void
DnsBuffer::cancel(void* requester) {
  assert(std::this_thread::get_id() == ThreadNet::thread_net()->thread_id());

  if (requester == nullptr)
    throw internal_error("DnsBuffer::cancel() called with null requester");

  auto initial_count = m_active_query_count;

  for (auto& query : m_active_queries) {
    if (query.requester == requester) {
      query = DnsBufferQuery{};
      m_active_query_count--;
    }
  }

  auto remove_itr = std::remove_if(m_pending_queries.begin(), m_pending_queries.end(), [requester](const auto& query) {
      return query.requester == requester;
    });

  m_pending_queries.erase(remove_itr, m_pending_queries.end());
  m_requesters.erase(requester);

  // TODO: Replace 'requester' with address of active array slot.

  if (initial_count != m_active_query_count)
    ThreadNet::thread_net()->dns_resolver()->cancel(requester);
}

void
DnsBuffer::activate_query(DnsBufferQuery query) {
  assert(std::this_thread::get_id() == ThreadNet::thread_net()->thread_id());

  if (m_active_query_count >= max_requests)
    throw internal_error("DnsBuffer::activate_query() m_active_query_count >= max_requests");

  if (query.callback == nullptr)
    throw internal_error("DnsBuffer::activate_query() called with null callback");

  auto itr = std::find_if(m_active_queries.begin(), m_active_queries.end(), [](const auto& query) {
      return query.callback == nullptr;
    });

  if (itr == m_active_queries.end())
    throw internal_error("DnsBuffer::activate_query() itr == m_active_queries.end()");

  *itr = std::move(query);
  m_active_query_count++;

  auto index = std::distance(m_active_queries.begin(), itr);

  auto fn = [this, index](sin_shared_ptr result_sin, sin6_shared_ptr result_sin6, int error) {
      process(index, std::move(result_sin), std::move(result_sin6), error);
    };

  ThreadNet::thread_net()->dns_resolver()->resolve(itr->requester, itr->hostname, itr->family, std::move(fn));
}

void
DnsBuffer::process(unsigned int index, sin_shared_ptr result_sin, sin6_shared_ptr result_sin6, int error) {
  assert(std::this_thread::get_id() == ThreadNet::thread_net()->thread_id());

  if (index >= m_active_queries.size())
    throw internal_error("DnsBuffer::process() index out of bounds");

  // TODO: This is wrong, as it doesn't handle canceled queries correctly.

  auto callback = std::move(m_active_queries[index].callback);

  m_active_queries[index] = DnsBufferQuery{};
  m_active_query_count--;

  callback(std::move(result_sin), std::move(result_sin6), error);

  if (!m_pending_queries.empty()) {
    activate_query(std::move(m_pending_queries.front()));
    m_pending_queries.pop_front();
  }
}

} // namespace torrent::net

