#include "config.h"

#include "udp_router.h"

#include "torrent/net/fd.h"
#include "torrent/net/network_config.h"
#include "torrent/net/poll.h"
#include "torrent/net/socket_address.h"
#include "torrent/runtime/socket_manager.h"
#include "torrent/utils/log.h"

#define LT_LOG(log_fmt, ...)                                            \
  lt_log_print_subsystem(LOG_TRACKER_REQUESTS, "udp_router", log_fmt, __VA_ARGS__);
  // lt_log_print_subsystem(LOG_TRACKER_REQUESTS, "udp_router", "%p : " log_fmt, static_cast<TrackerWorker*>(this), __VA_ARGS__);

namespace torrent::tracker {

void
UdpRouter::open(int family) {
  if (is_open())
    return;

  if (family != AF_INET)
    throw internal_error("UdpRouter::open() called with unsupported family.");

  auto bind_address = config::network_config()->bind_address_for_connect(AF_INET);

  if (bind_address == nullptr) {
    LT_LOG("could not open udp router : blocked or invalid bind address : family:%s", family_str(family));
    return;
  }

  auto open_fd = [this, family, &bind_address]() {
      int fd = fd_open_family(fd_flag_datagram | fd_flag_nonblock, family);

      if (fd == -1) {
        LT_LOG("opening router failed : open failed : family:%s errno:%s", family_str(family), system::errno_enum_str(errno).c_str());
        return;
      }

      if (!fd_bind(fd, bind_address.get())) {
        LT_LOG("opening router failed : bind failed : family:%s bind_address:%s errno:%s",
               family_str(family), sa_pretty_str(bind_address.get()).c_str(), system::errno_enum_str(errno).c_str());
        fd_close(fd);
        return;
      }

      set_file_descriptor(fd);

      this_thread::event_open(this);
      this_thread::event_insert_read(this);
      // this_thread::event_insert_write(this);
      this_thread::event_insert_error(this);
    };

  auto cleanup_func = [this, family]() {
      if (!is_open())
        return;

      LT_LOG("opening router failed : socket manager triggered cleanup : family:%s", family_str(family));

      this_thread::event_remove_and_close(this);

      fd_close(file_descriptor());
      set_file_descriptor(-1);
    };

  if (!runtime::socket_manager()->open_event_or_cleanup(this, open_fd, cleanup_func))
    return;

  LT_LOG("opened udp router : family:%s bind_address:%s", family_str(family), sa_pretty_str(bind_address.get()).c_str());
}

void
UdpRouter::close() {
  if (!is_open())
    return;

  runtime::socket_manager()->close_event_or_throw(this, [this]() {
      this_thread::event_remove_and_close(this);

      fd_close(file_descriptor());
      set_file_descriptor(-1);
    });

  LT_LOG("closed udp router", 0);
}

