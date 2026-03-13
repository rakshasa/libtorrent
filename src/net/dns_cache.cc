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

inline void
DnsCacheInfo::reset_state() {
  updating           = false;
  last_updated       = std::chrono::minutes{0};
  last_failed_update = std::chrono::minutes{0};
}

inline void
DnsCacheInfo::reset_updated(std::chrono::minutes current_time) {
  last_updated       = current_time;
  last_failed_update = std::chrono::minutes{0};
}

inline void
DnsCacheInfo::reset_failed(std::chrono::minutes current_time) {
  last_updated       = std::chrono::minutes{0};
  last_failed_update = current_time;
}

inline void
DnsCacheEntry::reset_updating(int family) {
  if (family == AF_INET || family == AF_UNSPEC)
    sin_info.updating = false;

  if (family == AF_INET6 || family == AF_UNSPEC)
    sin6_info.updating = false;
}

// TODO: When we have a partial match with on AF_UNSPEC requests, should we wrap the callback?
//
// TODO: We should wrap the callback, then pass the partial match if we receive an error, or both if we receive a success on the other family.

// TODO: Add a helper function to convert family int to cstring.

void
DnsCache::resolve(void* requester, const std::string& hostname, int family, resolver_callback&& callback) {
  if (family != AF_INET && family != AF_INET6 && family != AF_UNSPEC)
    throw internal_error("DnsCache::resolve() invalid address family");

  auto itr = m_cache.find(hostname);

  if (itr == m_cache.end()) {
    // No need to log.
    ThreadNet::thread_net()->dns_buffer()->resolve(requester, hostname, family, std::move(callback));
    return;
  }

  // If not stale, and (in case of AF_UNSPEC) both address families are present we should do nothing.
  //
  // If however a certain number of attempts have been made to get both address families, and we
  // have only received one, we should assume the other doesn't exist?

  // auto current_time = std::chrono::duration_cast<std::chrono::minutes>(this_thread::cached_time());

  // check if we're failed, if so attempt new resolve after certain amount of time.
  //
  // we should differentiate between 'failed to resolve' and 'resolved but no addresses'.

  // if (itr->second.last_failed_update != std::chrono::minutes{0}) {

  //   if (itr->second.no_record) {
  //     if (current_time < itr->second.last_failed_update + 60min) {
  //       LT_LOG_REQUESTER("matched cache entry with no record, returning no record error : hostname:%s family:%d", hostname.c_str(), family);

  //       this_thread::callback(requester, [callback = std::move(callback)]() {
  //           callback(nullptr, nullptr, EAI_NONAME);
  //         });

  //       return;
  //     }

  //     LT_LOG_REQUESTER("matched cache entry with no record, retrying : hostname:%s family:%d", hostname.c_str(), family);

  //     ThreadNet::thread_net()->dns_buffer()->resolve(requester, hostname, family, std::move(callback));
  //     return;
  //   }

  //   if (current_time < itr->second.last_failed_update + 10min) {
  //     LT_LOG_REQUESTER("matched cache entry with failed update, returning error : hostname:%s family:%d", hostname.c_str(), family);

  //     this_thread::callback(requester, [callback = std::move(callback)]() {
  //         callback(nullptr, nullptr, EAI_AGAIN);
  //       });

  //     return;
  //   }

  //   LT_LOG_REQUESTER("matched cache entry with failed update, retrying : hostname:%s family:%d", hostname.c_str(), family);

  //   ThreadNet::thread_net()->dns_buffer()->resolve(requester, hostname, family, std::move(callback));
  //   return;
  // }

  // This is currently just a staleness check, we should have logic to report back when addresses
  // are successfully connected to.
  //
  // Or rather, we should pass a 'failed' counter in the resolve to indicate how many failed
  // attempts have been made to connect to the address.

  // if (current_time > itr->second.last_updated + 24h) {
  //   LT_LOG_REQUESTER("stale cache entry, refreshing in background : hostname:%s family:%d", hostname.c_str(), family);

  //   ThreadNet::thread_net()->dns_buffer()->resolve(this, hostname, family, [](sin_shared_ptr, sin6_shared_ptr, int) {});
  //   itr->second.updating = true;
  // }

  /////////////////

  // auto& sin  = itr->second.sin_addr;
  // auto& sin6 = itr->second.sin6_addr;

  // LT_LOG_REQUESTER("matched cache entry, returning : hostname:%s family:%d sin:%s sin6:%s",
  //                  hostname.c_str(), family,
  //                  sin_pretty_or_empty(sin.get()).c_str(),
  //                  sin6_pretty_or_empty(sin6.get()).c_str());

  // this_thread::callback(requester, [sin, sin6, callback = std::move(callback)]() {
  //     callback(sin, sin6, 0);
  //   });

  // Just rewrite the simple callback for now:

  auto& sin_addr  = itr->second.sin_addr;
  auto& sin_info  = itr->second.sin_info;
  auto& sin6_addr = itr->second.sin6_addr;
  auto& sin6_info = itr->second.sin6_info;

  // We need to get the status of each address family (if they're wanted), then construct the callback last:

  auto fn = [](bool has_addr, const DnsCacheInfo& info) {
      if (!has_addr) {
        if (info.updating)
          return EAI_AGAIN;

        if (info.no_record)
          // Retry after a long while.
          return EAI_NONAME;

        // Queue a background request if failed timeout has passed, but return EAI_AGAIN as we're in
        // failure mode and the caller is going to retry.
        //
        // If it's been very long since the last try, we will use a normal request.

        return EAI_AGAIN;
      }

      return 0;
    };

  // Rewrite this whole thing to be simpler by explicitly handling each family type

  if (family == AF_INET) {
    switch (fn(sin_addr != nullptr, sin_info)) {
    case 0:
      LT_LOG_REQUESTER("matched cache entry, returning : hostname:%s family:%d sin:%s",
                       hostname.c_str(), family,
                       sin_pretty_or_empty(sin_addr.get()).c_str());

      this_thread::callback(requester, [sin_addr, callback = std::move(callback)]() {
          callback(sin_addr, nullptr, 0);
        });
      return;

    case EAI_NONAME:
      LT_LOG_REQUESTER("matched cache entry, returning no record error : hostname:%s family:%d", hostname.c_str(), family);

      this_thread::callback(requester, [callback = std::move(callback)]() {
          callback(nullptr, nullptr, EAI_NONAME);
        });
      return;

    case EAI_AGAIN:
      LT_LOG_REQUESTER("matched cache entry, but sin_addr is not available, resolving in background : hostname:%s family:%d", hostname.c_str(), family);

      ThreadNet::thread_net()->dns_buffer()->resolve(requester, hostname, AF_INET, std::move(callback));
      return;

    default:
      throw internal_error("DnsCache::resolve() invalid status for AF_INET");
    }
  }

  if (family == AF_INET6) {
    switch (fn(sin6_addr != nullptr, sin6_info)) {
    case 0:
      LT_LOG_REQUESTER("matched cache entry, returning : hostname:%s family:%d sin6:%s",
                       hostname.c_str(), family,
                       sin6_pretty_or_empty(sin6_addr.get()).c_str());

      this_thread::callback(requester, [sin6_addr, callback = std::move(callback)]() {
          callback(nullptr, sin6_addr, 0);
        });
      return;

    case EAI_NONAME:
      LT_LOG_REQUESTER("matched cache entry, returning no record error : hostname:%s family:%d", hostname.c_str(), family);

      this_thread::callback(requester, [callback = std::move(callback)]() {
          callback(nullptr, nullptr, EAI_NONAME);
        });
      return;

    case EAI_AGAIN:
      LT_LOG_REQUESTER("matched cache entry, but sin6_addr is not available, resolving in background : hostname:%s family:%d", hostname.c_str(), family);

      ThreadNet::thread_net()->dns_buffer()->resolve(requester, hostname, AF_INET6, std::move(callback));
      return;

    default:
      throw internal_error("DnsCache::resolve() invalid status for AF_INET6");
    }
  }

  // AF_UNSPEC:

  int status_sin  = fn(sin_addr != nullptr, sin_info);
  int status_sin6 = fn(sin6_addr != nullptr, sin6_info);

  if ((status_sin == 0 && status_sin6 == 0) ||
      (status_sin == 0 && status_sin6 == EAI_NONAME) ||
      (status_sin == EAI_NONAME && status_sin6 == 0)) {

    LT_LOG_REQUESTER("matched cache entry, returning : hostname:%s family:%d sin:%s sin6:%s",
                     hostname.c_str(), family,
                     sin_pretty_or_empty(sin_addr.get()).c_str(),
                     sin6_pretty_or_empty(sin6_addr.get()).c_str());

    this_thread::callback(requester, [sin_addr, sin6_addr, callback = std::move(callback)]() {
        callback(sin_addr, sin6_addr, 0);
      });
    return;
  }

  if (status_sin == EAI_NONAME && status_sin6 == EAI_NONAME) {
    LT_LOG_REQUESTER("matched cache entry, returning no record error : hostname:%s family:%d", hostname.c_str(), family);

    this_thread::callback(requester, [callback = std::move(callback)]() {
        callback(nullptr, nullptr, EAI_NONAME);
      });
    return;
  }

  if (status_sin == 0 && status_sin6 == EAI_AGAIN) {
    LT_LOG_REQUESTER("matched cache entry, returning : hostname:%s family:%d sin:%s sin6:EAI_AGAIN",
                     hostname.c_str(), family,
                     sin_pretty_or_empty(sin_addr.get()).c_str());

    this_thread::callback(requester, [sin_addr, callback = std::move(callback)]() {
        callback(sin_addr, nullptr, 0);
      });

    return;
  }

  if (status_sin == EAI_AGAIN && status_sin6 == 0) {
    LT_LOG_REQUESTER("matched cache entry, returning : hostname:%s family:%d sin:EAI_AGAIN sin6:%s",
                     hostname.c_str(), family,
                     sin6_pretty_or_empty(sin6_addr.get()).c_str());

    this_thread::callback(requester, [sin6_addr, callback = std::move(callback)]() {
        callback(nullptr, sin6_addr, 0);
      });

    return;
  }

  if (status_sin == EAI_AGAIN && status_sin6 == EAI_AGAIN) {
    // Return error

    LT_LOG_REQUESTER("matched cache entry, returning : hostname:%s family:%d sin:EAI_AGAIN sin6:EAI_AGAIN",
                     hostname.c_str(), family);

    this_thread::callback(requester, [callback = std::move(callback)]() {
        callback(nullptr, nullptr, EAI_AGAIN);
      });

    return;
  }

  throw internal_error("DnsCache::resolve() invalid status for AF_UNSPEC");
}

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

  auto [itr, inserted] = m_cache.try_emplace(hostname, DnsCacheEntry{});

  auto& sin_addr  = itr->second.sin_addr;
  auto& sin_info  = itr->second.sin_info;
  auto& sin6_addr = itr->second.sin6_addr;
  auto& sin6_info = itr->second.sin6_info;

  auto current_time = std::chrono::duration_cast<std::chrono::minutes>(this_thread::cached_time());

  if (inserted) {
    if (family == AF_INET || family == AF_UNSPEC) {
      sin_addr              = std::move(result_sin);
      sin_info.last_updated = current_time;
    }

    if (family == AF_INET6 || family == AF_UNSPEC) {
      sin6_addr              = std::move(result_sin6);
      sin6_info.last_updated = current_time;
    }

    LT_LOG("added new cache entry : hostname:%s family:%d sin:%s sin6:%s",
           hostname.c_str(), family,
           sin_pretty_or_empty(sin_addr.get()).c_str(),
           sin6_pretty_or_empty(sin6_addr.get()).c_str());
    return;
  }

  // TODO: Replace with clearing updating flag in reset_updated/failed. (would need to clear both in unspec)
  itr->second.reset_updating(family);

  if (family == AF_INET) {
    sin_addr           = std::move(result_sin);
    sin_info.no_record = false;

    sin_info.reset_updated(current_time);

    // TODO: Add helper functions to clear info.
    // TODO: Only log if address has changed?

    LT_LOG("updated cache entry : hostname:%s family:%d sin:%s",
           hostname.c_str(), family,
           sin_pretty_or_empty(sin_addr.get()).c_str());
    return;
  }

  if (family == AF_INET6) {
    sin6_addr           = std::move(result_sin6);
    sin6_info.no_record = false;

    sin6_info.reset_updated(current_time);

    LT_LOG("updated cache entry : hostname:%s family:%d sin6:%s",
           hostname.c_str(), family,
           sin6_pretty_or_empty(sin6_addr.get()).c_str());
    return;
  }

  // AF_UNSPEC:

  // It's too complicated to deal with possible errors from one of the families, so just ignore
  // the family that didn't get updated.

  if (result_sin) {
    sin_addr           = std::move(result_sin);
    sin_info.no_record = false;

    sin_info.reset_updated(current_time);
  }

  if (result_sin6) {
    sin6_addr           = std::move(result_sin6);
    sin6_info.no_record = false;

    sin6_info.reset_updated(current_time);
  }

  LT_LOG("updated cache entry : hostname:%s family:%d sin:%s sin6:%s",
         hostname.c_str(), family,
         sin_pretty_or_empty(sin_addr.get()).c_str(),
         sin6_pretty_or_empty(sin6_addr.get()).c_str());
}

void
DnsCache::process_failure(const std::string& hostname, int family, int error) {
  if (family != AF_INET && family != AF_INET6 && family != AF_UNSPEC)
    throw internal_error("DnsCache::process_failure() invalid address family");

  if (error == 0)
    throw internal_error("DnsCache::process_failure() error is 0, should be success");

  // TODO: If 'updating', add to scheduled task?

  auto [itr, inserted] = m_cache.try_emplace(hostname, DnsCacheEntry{});

  auto& sin_addr  = itr->second.sin_addr;
  auto& sin_info  = itr->second.sin_info;
  auto& sin6_addr = itr->second.sin6_addr;
  auto& sin6_info = itr->second.sin6_info;

  auto current_time = std::chrono::duration_cast<std::chrono::minutes>(this_thread::cached_time());

  if (inserted) {
    if (family == AF_INET) {
      switch (error) {
      case EAI_NONAME:
        // TODO: Should NONAME set success time?

        sin_info.no_record          = true;
        sin_info.last_failed_update = current_time;

        LT_LOG("added new cache entry with no record : hostname:%s family:AF_INET", hostname.c_str());
        return;

      default:
        sin_info.last_failed_update = current_time;

        LT_LOG("added new cache entry with failed update : hostname:%s family:AF_INET error:'%s'", hostname.c_str(), gai_strerror(error));
        return;
      };
    }

    if (family == AF_INET6) {
      switch (error) {
      case EAI_NONAME:
        sin6_info.no_record          = true;
        sin6_info.last_failed_update = current_time;

        LT_LOG("added new cache entry with no record : hostname:%s family:AF_INET6", hostname.c_str());
        return;

      default:
        sin6_info.last_failed_update = current_time;

        LT_LOG("added new cache entry with failed update : hostname:%s family:AF_INET6 error:'%s'", hostname.c_str(), gai_strerror(error));
        return;
      };
    }

    // AF_UNSPEC:

    sin_info.last_failed_update  = current_time;
    sin6_info.last_failed_update = current_time;

    // EAI_NONAME: We don't know if both are non-existent, so do separate updates for each family.

    // TODO: Do background updates here for EAI_NONAME

    LT_LOG("added new cache entry with (possibly) no record : hostname:%s family:AF_UNSPEC", hostname.c_str());
    return;
  }

  // We update the cache entry even if it doesn't exist to remove stale addresses.

  // TODO: EAI_NONAME should check if we're already set to no_record before doing anything.

  itr->second.reset_updating(family);

  if (family == AF_INET) {
    switch (error) {
    case EAI_NONAME:
      sin_addr           = nullptr;
      sin_info.no_record = true;

      // TODO: Should NONAME set success time?
      sin_info.reset_failed(current_time);

      LT_LOG("updated cache entry with no record : hostname:%s family:AF_INET", hostname.c_str());
      return;

    default:
      sin_info.reset_failed(current_time);

      LT_LOG("updated cache entry with failed update : hostname:%s family:AF_INET error:'%s'", hostname.c_str(), family, gai_strerror(error));
      return;
    };
  }

  if (family == AF_INET6) {
    switch (error) {
    case EAI_NONAME:
      sin6_addr           = nullptr;
      sin6_info.no_record = true;

      // TODO: Should NONAME set success time?
      sin6_info.reset_failed(current_time);

      LT_LOG("updated cache entry with no record : hostname:%s family:AF_INET6", hostname.c_str());
      return;

    default:
      sin6_info.reset_failed(current_time);

      LT_LOG("updated cache entry with failed update : hostname:%s family:AF_INET6 error:'%s'", hostname.c_str(), family, gai_strerror(error));
      return;
    };
  }

  // AF_UNSPEC:

  switch (error) {
  case EAI_NONAME:
    // TODO: We don't know if both are non-existent, so do separate updates for each family.

    // TODO: Check info.no_record == true, if so do nothing.

    // Just do naive failed counter update for now
    sin_info.reset_failed(current_time);
    sin6_info.reset_failed(current_time);

    LT_LOG("updated cache entry with no record : hostname:%s family:AF_UNSPEC", hostname.c_str());
    return;

  default:
    sin_info.reset_failed(current_time);
    sin6_info.reset_failed(current_time);

    // TODO: Do separate updates for each family here as well if it's been a while since last time?

    // TODO: However should we not just rely on initial insert to handle errors aggressively, and
    // then just let nature take its course with the updates?

    LT_LOG("updated cache entry with failed update : hostname:%s family:AF_UNSPEC error:'%s'", hostname.c_str(), family, gai_strerror(error));
    return;
  }
}

} // namespace torrent::net
