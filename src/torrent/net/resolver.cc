#include "config.h"

#include "torrent/net/resolver.h"

#include "rak/address_info.h"

#include "torrent/utils/thread.h"

namespace torrent::net {

void
Resolver::init() {
  m_thread_id = std::this_thread::get_id();

  m_signal_process_results = thread_self->signal_bitfield()->add_signal([this]() {
      process_results();
    });
}

void
Resolver::resolve(void* requester, const char* host, int family, int socktype, result_func slot) {
  if (std::this_thread::get_id() != m_thread_id)
    throw std::runtime_error("Resolver::resolve() called from wrong thread.");

  // Request request = { host, family, socktype, slot };

  // m_requests.push_back(request);

  // TODO: Temporary implementation.

  rak::address_info* ai;
  int err;

  if ((err = rak::address_info::get_address_info(host, family, socktype, &ai)) != 0) {
    m_results.push_back({ requester, NULL, err, slot });

  } else {
    rak::socket_address sa;
    sa.copy(*ai->address(), ai->length());
    rak::address_info::free_address_info(ai);

    m_results.push_back({ requester, sa.c_sockaddr(), 0, slot });
  }

  torrent::thread_self->signal_bitfield()->signal(m_signal_process_results);
}

void
Resolver::cancel(void* requester) {
  if (std::this_thread::get_id() != m_thread_id)
    throw std::runtime_error("Resolver::cancel() called from wrong thread.");

  m_requests.erase(std::remove_if(m_requests.begin(), m_requests.end(), [requester](const auto& request) {
        return request.requester == requester;
      }), m_requests.end());

  m_results.erase(std::remove_if(m_results.begin(), m_results.end(), [requester](const auto& result) {
        return result.requester == requester;
      }), m_results.end());
}

void
Resolver::process_results() {
  if (std::this_thread::get_id() != m_thread_id)
    throw std::runtime_error("Resolver::process_results() called from wrong thread.");

  auto results = std::move(m_results);

  for (auto& result : m_results) {
    result.slot(result.addr, result.err);
  }
}

} // namespace
