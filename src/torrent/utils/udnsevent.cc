#include <torrent/common.h>
#include "config.h"

#ifdef HAVE_UDNS

#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "udnsevent.h"
#include "globals.h"
#include "manager.h"
#include "torrent/poll.h"

namespace torrent {

    int udnserror_to_gaierror(int udnserror) {
        switch (udnserror) {
            case DNS_E_TEMPFAIL:
                return EAI_AGAIN;
            case DNS_E_PROTOCOL:
                // this isn't quite right
                return EAI_FAIL;
            case DNS_E_NXDOMAIN:
                return EAI_NONAME;
            case DNS_E_NODATA:
                return EAI_ADDRFAMILY;
            case DNS_E_NOMEM:
                return EAI_MEMORY;
            case DNS_E_BADQUERY:
                // again, not quite right
                return EAI_FAIL;
            default:
                return EAI_ADDRFAMILY;
        }
    }

    /** Compatibility layers so udns can call std::function callbacks. */

    void a4_callback_wrapper(struct ::dns_ctx *ctx, ::dns_rr_a4 *result, void *data) {
        struct sockaddr_in sa;
        const sockaddr *sa_addr = (sockaddr *) &sa;
        UdnsQuery *query = (UdnsQuery *) data;
        // udns will free the a4_query after this callback exits
        query->a4_query = NULL;

        if (result == NULL || result->dnsa4_nrr == 0) {
            if (query->a6_query == NULL) {
                // nothing more to do: call the callback with a failure status
                (*(query->callback))(NULL, udnserror_to_gaierror(::dns_status(ctx)));
                delete query;
            }
            // else: return and wait to see if we get an a6 response
        } else {
            sa.sin_family = AF_INET;
            sa.sin_port = 0;
            sa.sin_addr = result->dnsa4_addr[0];
            (*query->callback)(sa_addr, 0);
            if (query->a6_query != NULL) {
                ::dns_cancel(ctx, query->a6_query);
                delete query;
            }
        }
    }

    void a6_callback_wrapper(struct ::dns_ctx *ctx, ::dns_rr_a6 *result, void *data) {
        struct sockaddr_in6 sa;
        const sockaddr *sa_addr = (sockaddr *) &sa;
        UdnsQuery *query = (UdnsQuery *) data;
        // udns will free the a6_query after this callback exits
        query->a6_query = NULL;

        if (result == NULL || result->dnsa6_nrr == 0) {
            if (query->a4_query == NULL) {
                // nothing more to do: call the callback with a failure status
                (*(query->callback))(NULL, udnserror_to_gaierror(::dns_status(ctx)));
                delete query;
            }
            // else: return and wait to see if we get an a6 response
        } else {
            sa.sin6_family = AF_INET6;
            sa.sin6_port = 0;
            sa.sin6_addr = result->dnsa6_addr[0];
            (*query->callback)(sa_addr, 0);
            if (query->a4_query != NULL) {
                ::dns_cancel(ctx, query->a4_query);
                delete query;
            }
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
          throw internal_error("dns_init failed");
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
    }

    struct UdnsQuery *
    UdnsEvent::enqueue_resolve(const char *name, int family, resolver_callback *callback) {
        struct UdnsQuery *query = new UdnsQuery;
        query->a4_query = query->a6_query = NULL;
        query->callback = callback;

        if (family == AF_INET || family == AF_UNSPEC) {
            query->a4_query = ::dns_submit_a4(m_ctx, name, 0, a4_callback_wrapper, query);
            if (query->a4_query == NULL) throw new internal_error("dns_submit_a4 failed");
        }

        if (family == AF_INET6 || family == AF_UNSPEC) {
            query->a6_query = ::dns_submit_a6(m_ctx, name, 0, a6_callback_wrapper, query);
            if (query->a6_query == NULL) throw new internal_error("dns_submit_a6 failed");
        }

        return query;
    }

    void
    UdnsEvent::flush_resolves() {
        process_timeouts();
    }

    void
    UdnsEvent::process_timeouts() {
        int timeout = ::dns_timeouts(m_ctx, -1, 0);
        if (timeout != -1) {
            manager->poll()->insert_read(this);
            manager->poll()->insert_error(this);
            priority_queue_erase(&taskScheduler, &m_taskTimeout);
            priority_queue_insert(&taskScheduler, &m_taskTimeout, (cachedTime + rak::timer::from_seconds(timeout)).round_seconds());
        } else {
            // no pending queries
            manager->poll()->remove_read(this);
            manager->poll()->remove_error(this);
        }
    }

    void
    UdnsEvent::cancel(struct UdnsQuery *query) {
        if (query == NULL) {
            return;
        }
        if (query->a4_query != NULL) {
            ::dns_cancel(m_ctx, query->a4_query);
        }
        if (query->a6_query != NULL) {
            ::dns_cancel(m_ctx, query->a6_query);
        }
        delete query;
    }

    const char *
    UdnsEvent::type_name() {
        return "UdnsEvent";
    }

}

#endif
