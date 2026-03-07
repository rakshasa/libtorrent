#include "config.h"

#include "net/dns_cache.h"

#include <netdb.h>

#include "net/thread_net.h"
#include "net/dns_buffer.h"
// #include "torrent/common.h"
// #include "torrent/exceptions.h"
#include "torrent/net/socket_address.h"
#include "torrent/utils/log.h"

#define LT_LOG(log_fmt, ...)                                            \
  lt_log_print(LOG_NET_DNS, "dns-cache : " log_fmt, __VA_ARGS__);
#define LT_LOG_REQUESTER(log_fmt, ...)                                  \
  lt_log_print(LOG_NET_DNS, "%016p->dns-cache : " log_fmt, requester, __VA_ARGS__);

namespace torrent::net {

// TODO: Add callback slot to buffer.

// We match family and hostname together as the key, as we want to know both A and AAAA records, and
// don't want to do complex handling of partial results.

void
DnsCache::resolve(void* requester, const std::string& hostname, int family, resolver_callback&& callback) {
  auto itr = m_cache.find(DnsKey{family, hostname});

  if (itr == m_cache.end()) {
    // No need to log.
    ThreadNet::thread_net()->dns_buffer()->resolve(requester, hostname, family, std::move(callback));
    return;
  }

  // If not stale, and (in case of AF_UNSPEC) both address families are present we should do nothing.
  //
  // If however a certain number of attempts have been made to get both address families, and we
  // have only received one, we should assume the other doesn't exist?

  auto current_time = std::chrono::duration_cast<std::chrono::minutes>(this_thread::cached_time());

  // check if we're failed, if so attempt new resolve after certain amount of time.
  //
  // we should differentiate between 'failed to resolve' and 'resolved but no addresses'.

  if (itr->second.last_failed_update != std::chrono::minutes{0}) {

    if (itr->second.no_record) {
      if (current_time < itr->second.last_failed_update + 60min) {
        LT_LOG_REQUESTER("matched cache entry with no record, returning no record error : hostname:%s family:%d", hostname.c_str(), family);

        this_thread::callback(requester, [callback = std::move(callback)]() {
            callback(nullptr, nullptr, EAI_NONAME);
          });

        return;
      }

      LT_LOG_REQUESTER("matched cache entry with no record, retrying : hostname:%s family:%d", hostname.c_str(), family);

      ThreadNet::thread_net()->dns_buffer()->resolve(requester, hostname, family, std::move(callback));
      return;
    }

    if (current_time < itr->second.last_failed_update + 10min) {
      LT_LOG_REQUESTER("matched cache entry with failed update, returning error : hostname:%s family:%d", hostname.c_str(), family);

      this_thread::callback(requester, [callback = std::move(callback)]() {
          callback(nullptr, nullptr, EAI_AGAIN);
        });

      return;
    }

    LT_LOG_REQUESTER("matched cache entry with failed update, retrying : hostname:%s family:%d", hostname.c_str(), family);

    ThreadNet::thread_net()->dns_buffer()->resolve(requester, hostname, family, std::move(callback));
    return;
  }

  // This is currently just a staleness check, we should have logic to report back when addresses
  // are successfully connected to.
  //
  // Or rather, we should pass a 'failed' counter in the resolve to indicate how many failed
  // attempts have been made to connect to the address.

  if (current_time > itr->second.last_updated + 24h) {
    LT_LOG_REQUESTER("stale cache entry, refreshing in background : hostname:%s family:%d", hostname.c_str(), family);

    ThreadNet::thread_net()->dns_buffer()->resolve(this, hostname, family, [](sin_shared_ptr, sin6_shared_ptr, int) {});
    itr->second.updating = true;
  }

  auto& sin  = itr->second.sin;
  auto& sin6 = itr->second.sin6;

  this_thread::callback(requester, [sin, sin6, callback = std::move(callback)]() {
      callback(sin, sin6, 0);
    });
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
  // TODO: If 'updating', add to scheduled task.

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

  // itr->second.sin                = nullptr;
  // itr->second.sin6               = nullptr;

  // LT_LOG("updated cache entry with failed update : hostname:%s family:%d error:'%s'", hostname.c_str(), family, gai_strerror(error));

  // TODO: Schedule new resolve? Or do we rely on new attempts?
}

} // namespace torrent::net
