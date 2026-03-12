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

  int sin_status{EAI_NONAME};
  int sin6_status{EAI_NONAME};

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

  // if (family == AF_INET || family == AF_UNSPEC) {
  //   sin_status = fn(sin_addr != nullptr, sin_info);
  // }

  // if (family == AF_INET6 || family == AF_UNSPEC) {
  //   sin6_status = fn(sin6_addr != nullptr, sin6_info);
  // }

  // if ((family == AF_INET && sin_status == 0) ||
  //     (family == AF_INET6 && sin6_status == 0) ||
  //     (family == AF_UNSPEC && sin_status == 0 && sin6_status == 0) ||
  //     (family == AF_UNSPEC && sin_status == 0 && sin6_status == EAI_NONAME) ||
  //     (family == AF_UNSPEC && sin_status == EAI_NONAME && sin6_status == 0)) {

  //   LT_LOG_REQUESTER("matched cache entry, returning : hostname:%s family:%d sin:%s sin6:%s",
  //                    hostname.c_str(), family,
  //                    sin_pretty_or_empty(sin_addr.get()).c_str(),
  //                    sin6_pretty_or_empty(sin6_addr.get()).c_str());

  //   this_thread::callback(requester, [sin_addr, sin6_addr, callback = std::move(callback)]() {
  //       callback(sin_addr, sin6_addr, 0);
  //     });

  //   return;
  // }

  // if ((family == AF_INET && sin_status == EAI_NONAME) ||
  //     (family == AF_INET6 && sin6_status == EAI_NONAME) ||
  //     (family == AF_UNSPEC && sin_status == EAI_NONAME && sin6_status == EAI_NONAME)) {

  //   LT_LOG_REQUESTER("matched cache entry, returning no record error : hostname:%s family:%d", hostname.c_str(), family);

  //   this_thread::callback(requester, [callback = std::move(callback)]() {
  //       callback(nullptr, nullptr, EAI_NONAME);
  //     });

  //   return;
  // }

  // // For now, do the simple force request of both families. Or this won't work.

  // // The single families are easy:

  // // This is wrong, we

  // if (family == AF_INET) {
  //   LT_LOG_REQUESTER("matched cache entry, but sin_addr is not available, resolving in background : hostname:%s family:%d", hostname.c_str(), family);

  //   ThreadNet::thread_net()->dns_buffer()->resolve(requester, hostname, AF_INET, std::move(callback));
  //   return;
  // }


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
  auto current_time = std::chrono::duration_cast<std::chrono::minutes>(this_thread::cached_time());

  auto [itr, inserted] = m_cache.try_emplace(DnsKey{family, hostname}, DnsCacheEntry{});

  if (inserted) {
    itr->second.sin          = std::move(result_sin);
    itr->second.sin6         = std::move(result_sin6);
    itr->second.last_updated = current_time;

    LT_LOG("added new cache entry : hostname:%s family:%d", hostname.c_str(), family);
    return;
  }

  // Add checks here, e.g compare addresses to see if we should log new addresses, etc.

  // Also, if one of these is empty, yet was previously populated, we should handle that in a
  // special way such that repeated missing results should clear the old value as we probably aren't
  // failing to update, but rather the dns entry has been updated to remove one of the address families.

  if (result_sin)
    itr->second.sin = std::move(result_sin);

  if (result_sin6)
    itr->second.sin6 = std::move(result_sin6);

  itr->second.updating           = false;
  itr->second.no_record          = false;
  itr->second.last_updated       = current_time;
  itr->second.last_failed_update = std::chrono::minutes{0};

  LT_LOG("updated cache entry : hostname:%s family:%d sin:%s sin6:%s",
         hostname.c_str(), family,
         sin_pretty_or_empty(itr->second.sin.get()).c_str(),
         sin6_pretty_or_empty(itr->second.sin6.get()).c_str());
}

void
DnsCache::process_failure(const std::string& hostname, int family, int error) {
  // TODO: If 'updating', add to scheduled task?

  auto current_time = std::chrono::duration_cast<std::chrono::minutes>(this_thread::cached_time());

  auto [itr, inserted] = m_cache.try_emplace(DnsKey{family, hostname}, DnsCacheEntry{});

  itr->second.updating           = false;
  itr->second.last_updated       = std::chrono::minutes{0};
  itr->second.last_failed_update = current_time;

  if (inserted) {
    if (error == EAI_NONAME) {
      itr->second.no_record = true;

      LT_LOG("added new cache entry with no record : hostname:%s family:%d", hostname.c_str(), family);
      return;
    }

    LT_LOG("added new cache entry with failed update : hostname:%s family:%d error:'%s'", hostname.c_str(), family, gai_strerror(error));
    return;
  }

  if (error == EAI_NONAME) {
    if (itr->second.no_record) {
      LT_LOG("updated cache entry with no record, still no record : hostname:%s family:%d", hostname.c_str(), family);
      return;
    }

    itr->second.sin       = nullptr;
    itr->second.sin6      = nullptr;
    itr->second.no_record = true;

    LT_LOG("updated cache entry with no record : hostname:%s family:%d", hostname.c_str(), family);
    return;
  }

  LT_LOG("updated cache entry with failed update : hostname:%s family:%d error:'%s'", hostname.c_str(), family, gai_strerror(error));
}

} // namespace torrent::net
