#include "config.h"

#include "torrent/net/connection_state.h"

#include "torrent/exceptions.h"
#include "torrent/net/network_config.h"
#include "torrent/utils/log.h"

// #define LT_LOG_NOTICE(log_fmt, ...)                                     \
//   lt_log_print_subsystem(LOG_NOTICE, "net::network_config", log_fmt, __VA_ARGS__);

namespace torrent::net {

// function to check if we should retry other protocol immediately

int
connection_state_select_next_family(ConnectionState& state) {
  auto nc = config::network_config();

  // int current = state.current_family();

  // If current and no failures, keep using it.

  // Include prefer_ipv6

  // switch (state.current_family()) {
  // case AF_UNSPEC:
  //   if (nc->is_prefer_ipv6())
  //     return AF_INET6;
  //   else
  //     return AF_INET;

  // int preferred = nc->is_prefer_ipv6() ? AF_INET6 : AF_INET;



  // Check if last connection was successful, keep using

  // however we should always try the preferred again

  // If last connection failed, try the other family

  // If both families have failed, wait before trying again.


}

} // namespace torrent::net
