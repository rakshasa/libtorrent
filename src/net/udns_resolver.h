#ifndef LIBTORRENT_NET_UDNSEVENT_H
#define LIBTORRENT_NET_UDNSEVENT_H

#include <map>
#include <mutex>
#include <string>

#include "torrent/event.h"
#include "torrent/net/types.h"
#include "torrent/utils/scheduler.h"

struct dns_ctx;

namespace torrent {

struct UdnsQuery;
class  UdnsResolverInternal;

class UdnsResolver : public Event {
public:
  using resolver_callback = std::function<void(sin_shared_ptr, sin6_shared_ptr, int)>;

  using query_map = std::multimap<void*, std::unique_ptr<UdnsQuery>>;

  UdnsResolver();
  ~UdnsResolver() override;

  const char*         type_name() const override { return "udns"; }

  void                initialize(utils::Thread* thread);
  void                cleanup();

  // Callback must happen in thread_net and cannot call back into the resolver.
  //
  // If the hostname is a numeric address, it will result in the callback being called immediately.
  void                resolve(void* requester, const std::string& hostname, int family, resolver_callback&& callback);

  // Cancel may block if the resolver received the response and is calling the callback.
  void                cancel(void* requester);

  void                flush();

  void                event_read() override;
  void                event_write() override;
  void                event_error() override;

protected:
  friend class UdnsResolverInternal;

  std::unique_ptr<UdnsQuery> erase_query(query_map::iterator itr);

  query_map::iterator        find_query_or_fail_unsafe(UdnsQuery* query);

  bool                try_resolve_numeric(std::unique_ptr<UdnsQuery>& query);

  void                process_canceled();
  void                process_timeouts();

  static void         process_partial_result_unsafe(query_map::iterator itr);
  static void         process_final_result_unsafe(std::unique_ptr<UdnsQuery>&& query);

  static bool         m_initialized;

  utils::Thread*      m_thread{};
  ::dns_ctx*          m_ctx{};

  utils::SchedulerEntry m_task_timeout;

  std::mutex          m_mutex;
  query_map           m_queries_unsafe;
  query_map           m_malformed_queries_unsafe;
};

} // namespace torrent

#endif // LIBTORRENT_NET_UDNSEVENT_H
