#include "config.h"

#include "net/dns_cache.h"

#include <netdb.h>

#include "net/thread_net.h"
#include "net/dns_buffer.h"
#include "torrent/exceptions.h"
#include "torrent/net/socket_address.h"
#include "torrent/utils/log.h"

#define LT_LOG(log_fmt, ...)                                            \
  lt_log_print(LOG_NET_DNS, "dns-cache : " log_fmt, __VA_ARGS__);
#define LT_LOG_REQUESTER(log_fmt, ...)                                  \
  lt_log_print(LOG_NET_DNS, "%016p->dns-cache : " log_fmt, requester, __VA_ARGS__);

namespace torrent::net {

namespace {

enum {
  DNS_EMPTY,
  DNS_VALID,
  DNS_TRY_AGAIN,
  DNS_NO_RECORD,
};

int
dns_error(int status) {
  switch (status) {
  case DNS_TRY_AGAIN: return EAI_AGAIN;
  case DNS_NO_RECORD: return EAI_NONAME;
  default:            throw internal_error("dns_error() invalid status");
  }
}

const char*
dns_error_str(int status) {
  switch (status) {
  case DNS_TRY_AGAIN: return "EAI_AGAIN";
  case DNS_NO_RECORD: return "EAI_NONAME";
  default:            return "invalid";
  }
}

std::chrono::minutes
last_update_or_failed(const DnsCacheInfo& info) {
  return std::max(info.last_updated, info.last_failed_update);
}

} // namespace

// TODO: Change timer comparisons to use a a helper function that checks the latest valid time. (and possibly add internal errors)

void
DnsCache::resolve(void* requester, std::string hostname, int family, resolver_callback&& callback) {
  cull_stale_entries();

  if (hostname.empty())
    throw internal_error("DnsCache::resolve() hostname is empty");

  if (family != AF_INET && family != AF_INET6 && family != AF_UNSPEC)
    throw internal_error("DnsCache::resolve() invalid address family");

  auto itr = m_entries.find(hostname);

  if (itr == m_entries.end()) {
    // No need to log.
    ThreadNet::thread_net()->dns_buffer()->resolve(requester, hostname, family, std::move(callback));
    return;
  }

  auto& sin_addr  = itr->second->sin_addr;
  auto& sin_info  = itr->second->sin_info;
  auto& sin6_addr = itr->second->sin6_addr;
  auto& sin6_info = itr->second->sin6_info;

  auto current_time = std::chrono::duration_cast<std::chrono::minutes>(this_thread::cached_time());

  // We need to get the status of each address family (if they're wanted), then construct the callback last:

  auto fn = [&](bool has_addr, int family, DnsCacheInfo& info) {
      if (has_addr) {
        // This is currently just a staleness check, we should have logic to report back when addresses
        // are successfully connected to.
        //
        // Or rather, we should pass a 'failed' counter in the resolve to indicate how many failed
        // attempts have been made to connect to the address.

        if (current_time > last_update_or_failed(info) + 24h)
          update_stale_info("valid record", requester, hostname, family, info);

        return DNS_VALID;
      }

      if (last_update_or_failed(info) == std::chrono::minutes(0))
        return DNS_EMPTY;

      if (info.updating) {
        // TODO: Add sanity check on 'updating' here?

        // TODO: When updating, we add ourselves to the request unless it's a stale update or we
        // have very many failures / recent retries.

        return DNS_TRY_AGAIN;
      }

      if (info.no_record) {
        // TODO: Change to consider no_record a success.
        if (current_time > last_update_or_failed(info) + 2h)
          update_stale_info("no record", requester, hostname, family, info);

        return DNS_NO_RECORD;
      }

      // Queue a background request if failed timeout has passed, but return EAI_AGAIN as we're in
      // failure mode and the caller is going to retry.
      //
      // If it's been very long since the last try, we will use a normal request.
      //
      // Make sure we don't keep trying to resolve if we don't get proper responses.

      // TODO: Replace with 'failed' counter, and use that to determine when to retry, when to give up, etc.

      if (current_time > last_update_or_failed(info) + 10min)
        update_stale_info("failed update", requester, hostname, family, info);

      // TODO: Have a special code for when we're in try-again and also want to force a new resolve
      // due to staleness. (DSN_TRY_AGAIN_STALE)

      return DNS_TRY_AGAIN;
    };

  // TODO: Detect when either A or AAAA dns server isn't responding, and cut back on retries, etc.

  // AF_INET:

  if (family == AF_INET) {
    auto status_sin = fn(sin_addr != nullptr, AF_INET, sin_info);

    switch (status_sin) {
    case DNS_EMPTY:
      LT_LOG_REQUESTER("matched cache entry, resolving missing AF_INET : hostname:%s family:AF_INET", hostname.c_str());
      return queue_resolve(requester, hostname, AF_INET, sin_info, std::move(callback));

    case DNS_VALID:
      LT_LOG_REQUESTER("matched cache entry : hostname:%s family:AF_INET sin:%s", hostname.c_str(), sin_pretty_or_empty(sin_addr.get()).c_str());
      return callback(sin_addr, 0, nullptr, 0);

    case DNS_NO_RECORD:
    case DNS_TRY_AGAIN:
      LT_LOG_REQUESTER("matched cache entry : hostname:%s family:AF_INET sin:%s", hostname.c_str(), dns_error_str(status_sin));
      return callback(nullptr, dns_error(status_sin), nullptr, 0);
    }
  }

  // AF_INET6:

  if (family == AF_INET6) {
    auto status_sin6 = fn(sin6_addr != nullptr, AF_INET6, sin6_info);

    switch (status_sin6) {
    case DNS_EMPTY:
      LT_LOG_REQUESTER("matched cache entry, resolving missing AF_INET6 : hostname:%s family:AF_INET6", hostname.c_str());
      return queue_resolve(requester, hostname, AF_INET6, sin6_info, std::move(callback));

    case DNS_VALID:
      LT_LOG_REQUESTER("matched cache entry : hostname:%s family:AF_INET6 sin6:%s", hostname.c_str(), sin6_pretty_or_empty(sin6_addr.get()).c_str());
      return callback(nullptr, 0, sin6_addr, 0);

    case DNS_NO_RECORD:
    case DNS_TRY_AGAIN:
      LT_LOG_REQUESTER("matched cache entry : hostname:%s family:AF_INET6 sin6:%s", hostname.c_str(), dns_error_str(status_sin6));
      return callback(nullptr, 0, nullptr, dns_error(status_sin6));
    }
  }

  // AF_UNSPEC:

  auto status_sin  = fn(sin_addr != nullptr, AF_INET, sin_info);
  auto status_sin6 = fn(sin6_addr != nullptr, AF_INET6, sin6_info);

  if (status_sin == DNS_VALID) {
    switch (status_sin6) {
    case DNS_EMPTY:
      LT_LOG_REQUESTER("matched cache entry, resolving missing AF_INET6 : hostname:%s family:AF_UNSPEC sin:%s",
                       hostname.c_str(), sin_pretty_or_empty(sin_addr.get()).c_str());

      return queue_resolve(requester, hostname, AF_INET6, sin6_info, [sin_addr, callback = std::move(callback)](auto, int, auto sin6_addr, int sin6_error) mutable {
          return callback(sin_addr, 0, sin6_addr, sin6_error);
        });

    case DNS_VALID:
      LT_LOG_REQUESTER("matched cache entry : hostname:%s family:AF_UNSPEC sin:%s sin6:%s", hostname.c_str(),
                       sin_pretty_or_empty(sin_addr.get()).c_str(), sin6_pretty_or_empty(sin6_addr.get()).c_str());

      return callback(sin_addr, 0, sin6_addr, 0);

    case DNS_NO_RECORD:
    case DNS_TRY_AGAIN:
      LT_LOG_REQUESTER("matched cache entry : hostname:%s family:AF_UNSPEC sin:%s sin6:%s",
                       hostname.c_str(), sin_pretty_or_empty(sin_addr.get()).c_str(), dns_error_str(status_sin));

      return callback(sin_addr, 0, nullptr, dns_error(status_sin));
    };
  }

  if (status_sin6 == DNS_VALID) {
    switch (status_sin) {
    case DNS_EMPTY:
      LT_LOG_REQUESTER("matched cache entry, resolving missing AF_INET : hostname:%s family:AF_UNSPEC sin6:%s",
                       hostname.c_str(), sin6_pretty_or_empty(sin6_addr.get()).c_str());

      return queue_resolve(requester, hostname, AF_INET, sin_info, [sin6_addr, callback = std::move(callback)](auto sin_addr, int error_sin, auto, int) mutable {
          return callback(sin_addr, error_sin, sin6_addr, 0);
        });

    case DNS_NO_RECORD:
    case DNS_TRY_AGAIN:
      LT_LOG_REQUESTER("matched cache entry : hostname:%s family:AF_UNSPEC sin:%s sin6:%s",
                       hostname.c_str(), dns_error_str(status_sin), sin6_pretty_or_empty(sin6_addr.get()).c_str());

      return callback(nullptr, dns_error(status_sin), sin6_addr, 0);

    case DNS_VALID:
      throw internal_error("DnsCache::resolve() unreachable code : status_sin6 == DNS_VALID");
    };
  }

  if (status_sin == DNS_EMPTY) {
    switch (status_sin6) {
    case DNS_NO_RECORD:
    case DNS_TRY_AGAIN:
      LT_LOG_REQUESTER("matched cache entry, resolving missing AF_INET : hostname:%s family:AF_UNSPEC sin6:%s", hostname.c_str(), dns_error_str(status_sin6));
      return queue_resolve(requester, hostname, AF_INET, sin_info, std::move(callback));

    case DNS_EMPTY:
    case DNS_VALID:
      throw internal_error("DnsCache::resolve() unreachable code : status_sin == DNS_EMPTY");
    };
  }

  if (status_sin6 == DNS_EMPTY) {
    switch (status_sin) {
    case DNS_NO_RECORD:
    case DNS_TRY_AGAIN:
      LT_LOG_REQUESTER("matched cache entry, resolving missing AF_INET6 : hostname:%s family:AF_UNSPEC sin:%s", hostname.c_str(), dns_error_str(status_sin));
      return queue_resolve(requester, hostname, AF_INET6, sin6_info, std::move(callback));

    case DNS_EMPTY:
    case DNS_VALID:
      throw internal_error("DnsCache::resolve() unreachable code : status_sin6 == DNS_EMPTY");
    };
  }

  if (status_sin == DNS_NO_RECORD || status_sin6 == DNS_NO_RECORD) {
    LT_LOG_REQUESTER("matched cache entry, returning no record error : hostname:%s family:AF_UNSPEC sin:%s sin6:%s",
                     hostname.c_str(), dns_error_str(status_sin), dns_error_str(status_sin6));

    return callback(nullptr, dns_error(status_sin), nullptr, dns_error(status_sin6));
  }

  if (status_sin == DNS_TRY_AGAIN && status_sin6 == DNS_TRY_AGAIN) {
    LT_LOG_REQUESTER("matched cache entry, returning try again error : hostname:%s family:AF_UNSPEC sin:EAI_AGAIN sin6:EAI_AGAIN", hostname.c_str());

    return callback(nullptr, EAI_AGAIN, nullptr, EAI_AGAIN);
  }

  throw internal_error("DnsCache::resolve() unreachable code : end-of-function");
}

// TODO: When we add per-family errors, change this to only handle AF_INET/AF_INET6.

void
DnsCache::process_success(const std::string& hostname, int family, sin_shared_ptr result_sin, sin6_shared_ptr result_sin6) {
  if (family != AF_INET && family != AF_INET6 && family != AF_UNSPEC)
    throw internal_error("DnsCache::process_success() invalid address family");

  if (result_sin  == nullptr && result_sin6 == nullptr)
    throw internal_error("DnsCache::process_success() both result_sin and result_sin6 are nullptr");

  if (family == AF_INET && result_sin == nullptr)
    throw internal_error("DnsCache::process_success() result_sin is nullptr for AF_INET");

  if (family == AF_INET6 && result_sin6 == nullptr)
    throw internal_error("DnsCache::process_success() result_sin6 is nullptr for AF_INET6");

  auto [itr, inserted] = m_entries.try_emplace(hostname, nullptr);

  if (inserted)
    itr->second = std::make_unique<DnsCacheEntry>();

  auto& sin_addr  = itr->second->sin_addr;
  auto& sin_info  = itr->second->sin_info;
  auto& sin6_addr = itr->second->sin6_addr;
  auto& sin6_info = itr->second->sin6_info;

  auto current_time = std::chrono::duration_cast<std::chrono::minutes>(this_thread::cached_time());

  if (inserted) {
    sin_info.entry_itr      = itr;
    sin6_info.entry_itr     = itr;
    sin_info.staleness_itr  = m_sin_staleness.end();
    sin6_info.staleness_itr = m_sin6_staleness.end();

    if (family == AF_INET) {
      sin_addr = std::move(result_sin);
      reset_sin_updated(itr->second.get(), current_time);

      LT_LOG("added new cache entry : hostname:%s family:AF_INET sin:%s",
             hostname.c_str(), sin_pretty_or_empty(sin_addr.get()).c_str());
      return;
    }

    if (family == AF_INET6) {
      sin6_addr = std::move(result_sin6);
      reset_sin6_updated(itr->second.get(), current_time);

      LT_LOG("added new cache entry : hostname:%s family:AF_INET6 sin6:%s",
             hostname.c_str(), sin6_pretty_or_empty(sin6_addr.get()).c_str());
      return;
    }

    throw internal_error("DnsCache::process_success() unreachable code : inserted");
  }

  if (family == AF_INET) {
    sin_addr           = std::move(result_sin);
    sin_info.no_record = false;

    reset_sin_updated(itr->second.get(), current_time);

    // TODO: Only log if address has changed?
    LT_LOG("updated cache entry : hostname:%s family:AF_INET sin:%s",
           hostname.c_str(), sin_pretty_or_empty(sin_addr.get()).c_str());
    return;
  }

  if (family == AF_INET6) {
    sin6_addr           = std::move(result_sin6);
    sin6_info.no_record = false;

    reset_sin6_updated(itr->second.get(), current_time);

    LT_LOG("updated cache entry : hostname:%s family:AF_INET6 sin6:%s",
           hostname.c_str(), sin6_pretty_or_empty(sin6_addr.get()).c_str());
    return;
  }

  throw internal_error("DnsCache::process_success() unreachable code");
}

void
DnsCache::process_failure(const std::string& hostname, int family, int error) {
  if (family != AF_INET && family != AF_INET6 && family != AF_UNSPEC)
    throw internal_error("DnsCache::process_failure() invalid address family");

  if (error == 0)
    throw internal_error("DnsCache::process_failure() error is 0, should be success");

  // TODO: If 'updating', add to scheduled task?

  auto [itr, inserted] = m_entries.try_emplace(hostname, nullptr);

  if (inserted)
    itr->second = std::make_unique<DnsCacheEntry>();

  auto& sin_addr  = itr->second->sin_addr;
  auto& sin_info  = itr->second->sin_info;
  auto& sin6_addr = itr->second->sin6_addr;
  auto& sin6_info = itr->second->sin6_info;

  auto current_time = std::chrono::duration_cast<std::chrono::minutes>(this_thread::cached_time());

  if (inserted) {
    sin_info.entry_itr      = itr;
    sin6_info.entry_itr     = itr;
    sin_info.staleness_itr  = m_sin_staleness.end();
    sin6_info.staleness_itr = m_sin6_staleness.end();

    if (family == AF_INET) {
      switch (error) {
      case EAI_NONAME:
        // TODO: Should NONAME set success time?

        sin_info.no_record = true;
        reset_sin_failed(itr->second.get(), current_time);

        LT_LOG("added new cache entry with no record : hostname:%s family:AF_INET", hostname.c_str());
        return;

      default:
        reset_sin_failed(itr->second.get(), current_time);

        LT_LOG("added new cache entry with failed update : hostname:%s family:AF_INET error:'%s'", hostname.c_str(), gai_strerror(error));
        return;
      };
    }

    if (family == AF_INET6) {
      switch (error) {
      case EAI_NONAME:
        sin6_info.no_record = true;
        reset_sin6_failed(itr->second.get(), current_time);

        LT_LOG("added new cache entry with no record : hostname:%s family:AF_INET6", hostname.c_str());
        return;

      default:
        reset_sin6_failed(itr->second.get(), current_time);

        LT_LOG("added new cache entry with failed update : hostname:%s family:AF_INET6 error:'%s'", hostname.c_str(), gai_strerror(error));
        return;
      };
    }

    throw internal_error("DnsCache::process_failure() unreachable code : inserted");
  }

  // TODO: EAI_NONAME should check if we're already set to no_record before doing anything.

  if (family == AF_INET) {
    switch (error) {
    case EAI_NONAME:
      sin_addr           = nullptr;
      sin_info.no_record = true;

      // TODO: Should NONAME set success time?
      reset_sin_failed(itr->second.get(), current_time);

      LT_LOG("updated cache entry with no record : hostname:%s family:AF_INET", hostname.c_str());
      return;

    default:
      reset_sin_failed(itr->second.get(), current_time);

      LT_LOG("updated cache entry with failed update : hostname:%s family:AF_INET error:'%s'", hostname.c_str(), gai_strerror(error));
      return;
    };
  }

  if (family == AF_INET6) {
    switch (error) {
    case EAI_NONAME:
      sin6_addr           = nullptr;
      sin6_info.no_record = true;

      // TODO: Should NONAME set success time?
      reset_sin6_failed(itr->second.get(), current_time);

      LT_LOG("updated cache entry with no record : hostname:%s family:AF_INET6", hostname.c_str());
      return;

    default:
      reset_sin6_failed(itr->second.get(), current_time);

      LT_LOG("updated cache entry with failed update : hostname:%s family:AF_INET6 error:'%s'", hostname.c_str(), gai_strerror(error));
      return;
    };
  }

  throw internal_error("DnsCache::process_failure() unreachable code");
}

void
DnsCache::reset_sin_updated(DnsCacheEntry* entry, std::chrono::minutes current_time) {
  auto& info = entry->sin_info;

  info.updating           = false;
  info.last_updated       = current_time;
  info.last_failed_update = std::chrono::minutes{0};

  if (info.staleness_itr != m_sin_staleness.end())
    m_sin_staleness.erase(info.staleness_itr);

  info.staleness_itr = m_sin_staleness.insert(m_sin_staleness.end(), std::ref(entry));
}

void
DnsCache::reset_sin6_updated(DnsCacheEntry* entry, std::chrono::minutes current_time) {
  auto& info = entry->sin6_info;

  info.updating           = false;
  info.last_updated       = current_time;
  info.last_failed_update = std::chrono::minutes{0};

  if (info.staleness_itr != m_sin6_staleness.end())
    m_sin6_staleness.erase(info.staleness_itr);

  info.staleness_itr = m_sin6_staleness.insert(m_sin6_staleness.end(), std::ref(entry));
}

void
DnsCache::reset_sin_failed(DnsCacheEntry* entry, std::chrono::minutes current_time) {
  auto& info = entry->sin_info;

  info.updating           = false;
  info.last_updated       = std::chrono::minutes{0};
  info.last_failed_update = current_time;

  if (info.staleness_itr != m_sin_staleness.end())
    m_sin_staleness.erase(info.staleness_itr);

  info.staleness_itr = m_sin_staleness.insert(m_sin_staleness.end(), std::ref(entry));
}

void
DnsCache::reset_sin6_failed(DnsCacheEntry* entry, std::chrono::minutes current_time) {
  auto& info = entry->sin6_info;

  info.updating           = false;
  info.last_updated       = std::chrono::minutes{0};
  info.last_failed_update = current_time;

  if (info.staleness_itr != m_sin6_staleness.end())
    m_sin6_staleness.erase(info.staleness_itr);

  info.staleness_itr = m_sin6_staleness.insert(m_sin6_staleness.end(), std::ref(entry));
}

void
DnsCache::cull_stale_entries() {
  auto current_time = std::chrono::duration_cast<std::chrono::minutes>(this_thread::cached_time());

  while (!m_sin_staleness.empty()) {
    auto* entry = m_sin_staleness.front();
    auto& info  = entry->sin_info;

    if (current_time < last_update_or_failed(info) + 48h)
      break;

    LT_LOG("culling stale cache entry : hostname:%s family:AF_INET sin:%s",
           info.entry_itr->first.c_str(), sin_pretty_or_empty(entry->sin_addr.get()).c_str());

    m_entries.erase(info.entry_itr);
    m_sin_staleness.pop_front();
  }

  while (!m_sin6_staleness.empty()) {
    auto* entry = m_sin6_staleness.front();
    auto& info  = entry->sin6_info;

    if (current_time < last_update_or_failed(info) + 48h)
      break;

    LT_LOG("culling stale cache entry : hostname:%s family:AF_INET6 sin6:%s",
           info.entry_itr->first.c_str(), sin6_pretty_or_empty(entry->sin6_addr.get()).c_str());

    m_entries.erase(info.entry_itr);
    m_sin6_staleness.pop_front();
  }
}

void
DnsCache::queue_resolve(void* requester, const std::string& hostname, int family, DnsCacheInfo& info, resolver_callback&& callback) {
  ThreadNet::thread_net()->dns_buffer()->resolve(requester, hostname, family, std::move(callback));
  info.updating = true;
}

void
DnsCache::update_stale_info(const char* reason, void* requester, const std::string& hostname, int family, DnsCacheInfo& info) {
  LT_LOG_REQUESTER("stale cache entry with %s, retrying in background : hostname:%s family:%s",
                   reason, hostname.c_str(), family_str(family));

  ThreadNet::thread_net()->dns_buffer()->resolve(this, hostname, family, []( auto, int, auto, int) {});
  info.updating = true;
}

} // namespace torrent::net
