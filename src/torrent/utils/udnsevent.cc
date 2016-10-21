#include <netinet/in.h>

#include "config.h"
#include "udnsevent.h"
#include "globals.h"
#include "manager.h"
#include "torrent/poll.h"

namespace torrent {

    /** Compatibility layer so udns can call a std::function callback. */
    void a4_callback_wrapper(struct dns_ctx *ctx, dns_rr_a4 *result, void *data) {
        struct sockaddr_in sa;
        const sockaddr *sa_addr = (sockaddr *) &sa;
        UdnsEvent::a4_callback_type *callback = (UdnsEvent::a4_callback_type *) data;
        if (result == NULL || result->dnsa4_nrr == 0) {
            // XXX udns doesn't expose real errno's, so pass ENOENT
            (*callback)(NULL, 2);
        } else {
            sa.sin_family = AF_INET;
            sa.sin_port = 0;
            sa.sin_addr = result->dnsa4_addr[0];
            (*callback)(sa_addr, 0);
        }
    }

    UdnsEvent::UdnsEvent() {
      // reinitialize the default context, no-op
      // TODO don't do this here --- do it once in the manager, or in rtorrent
      ::dns_init(NULL, 0);
      // thread-safe context isolated to this object:
      m_ctx = ::dns_new(NULL);
      int fd = ::dns_init(m_ctx, 1);
      if (fd > 0) {
          m_fileDesc = fd;
      } else {
          throw internal_error("dns_init failed\n");
      }

      m_taskTimeout.slot() = std::bind(&UdnsEvent::process_timeouts, this);
    }

    UdnsEvent::~UdnsEvent() {
        priority_queue_erase(&taskScheduler, &m_taskTimeout);
        ::dns_close(m_ctx);
        ::dns_free(m_ctx);
    }

    void UdnsEvent::event_read() {
        ::dns_ioevent(m_ctx, 0);
    }

    void UdnsEvent::event_write() {
    }

    void UdnsEvent::event_error() {
        // TODO ?
    }

    struct ::dns_query *
    UdnsEvent::resolve_in4(const char *name, a4_callback_type *callback) {
        struct ::dns_query *query = dns_submit_a4(m_ctx, name, 0, a4_callback_wrapper, callback);
        if (query == NULL) {
            throw internal_error("dns_submit_a4 failed\n");
        }
        // this actually sends the packet
        process_timeouts();

        // don't listen for writes --- all writes are done by ::dns_timeouts
        manager->poll()->insert_read(this);
        manager->poll()->insert_error(this);

        return query;
    }

    void
    UdnsEvent::process_timeouts() {
        int timeout = ::dns_timeouts(m_ctx, -1, 0);
        if (timeout != -1) {
            priority_queue_erase(&taskScheduler, &m_taskTimeout);
            priority_queue_insert(&taskScheduler, &m_taskTimeout, (cachedTime + rak::timer::from_seconds(timeout)).round_seconds());
        } else {
            // no pending queries
            manager->poll()->remove_read(this);
            manager->poll()->remove_error(this);
        }
    }

    void
    UdnsEvent::cancel(struct ::dns_query *query) {
        ::dns_cancel(m_ctx, query);
    }

    const char *
    UdnsEvent::type_name() {
        return "UdnsEvent";
    }

}
