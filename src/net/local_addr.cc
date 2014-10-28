// libTorrent - BitTorrent library
// Copyright (C) 2005-2007, Jari Sundell
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
// In addition, as a special exception, the copyright holders give
// permission to link the code of portions of this program with the
// OpenSSL library under certain conditions as described in each
// individual source file, and distribute linked combinations
// including the two.
//
// You must obey the GNU General Public License in all respects for
// all of the code used other than OpenSSL.  If you modify file(s)
// with this exception, you may extend this exception to your version
// of the file(s), but you are not obligated to do so.  If you do not
// wish to do so, delete this exception statement from your version.
// If you delete this exception statement from all source files in the
// program, then also delete it here.
//
// Contact:  Jari Sundell <jaris@ifi.uio.no>
//
//           Skomakerveien 33
//           3185 Skoppum, NORWAY

#include "config.h"

#include <stdio.h>
#include <ifaddrs.h>
#include <rak/socket_address.h>
#include <sys/types.h>
#include <errno.h>

#ifdef __linux__
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#endif

#include "torrent/exceptions.h"
#include "socket_fd.h"
#include "local_addr.h"

namespace torrent {

#ifdef __linux__

namespace {

// IPv4 priority, from highest to lowest:
//
//   1. Everything else (global address)
//   2. Private address space (10.0.0.0/8, 172.16.0.0/12, 192.168.0.0/16)
//   3. Empty/INADDR_ANY (0.0.0.0)
//   4. Link-local address (169.254.0.0/16)
//   5. Localhost (127.0.0.0/8)
int
get_priority_ipv4(const in_addr& addr) {
  if ((addr.s_addr & htonl(0xff000000U)) == htonl(0x7f000000U)) {
    return 5;
  }
  if ((addr.s_addr & htonl(0xffff0000U)) == htonl(0xa9fe0000U)) {
    return 4;
  }
  if (addr.s_addr == htonl(0)) {
    return 3;
  }
  if ((addr.s_addr & htonl(0xff000000U)) == htonl(0x0a000000U) ||
      (addr.s_addr & htonl(0xfff00000U)) == htonl(0xac100000U) ||
      (addr.s_addr & htonl(0xffff0000U)) == htonl(0xc0a80000U)) {
    return 2;
  }
  return 1;
}

// IPv6 priority, from highest to lowest:
//
//  1. Global address (2000::/16 not in 6to4 or Teredo)
//  2. 6to4 (2002::/16)
//  3. Teredo (2001::/32)
//  4. Empty/INADDR_ANY (::)
//  5. Everything else (link-local, ULA, etc.)
int
get_priority_ipv6(const in6_addr& addr) {
  const uint32_t *addr32 = reinterpret_cast<const uint32_t *>(addr.s6_addr);
  if (addr32[0] == htonl(0) &&
      addr32[1] == htonl(0) &&
      addr32[2] == htonl(0) &&
      addr32[3] == htonl(0)) {
    return 4;
  }
  if (addr32[0] == htonl(0x20010000)) {
    return 3;
  }
  if ((addr32[0] & htonl(0xffff0000)) == htonl(0x20020000)) {
    return 2;
  }
  if ((addr32[0] & htonl(0xe0000000)) == htonl(0x20000000)) {
    return 1;
  }
  return 5;
}

int
get_priority(const rak::socket_address& addr) {
  switch (addr.family()) {
  case AF_INET:
    return get_priority_ipv4(addr.c_sockaddr_inet()->sin_addr);
  case AF_INET6:
    return get_priority_ipv6(addr.c_sockaddr_inet6()->sin6_addr);
  default:
    throw torrent::internal_error("Unknown address family given to compare");
  }
}

}

// Linux-specific implementation that understands how to filter away
// understands how to filter away secondary addresses.
bool get_local_address(sa_family_t family, rak::socket_address *address) {
  ifaddrs *ifaddrs;
  if (getifaddrs(&ifaddrs)) {
    return false;
  }

  rak::socket_address best_addr;
  switch (family) {
  case AF_INET:
    best_addr.sa_inet()->clear();
    break;
  case AF_INET6:
    best_addr.sa_inet6()->clear();
    break;
  default:
    throw torrent::internal_error("Unknown address family given to get_local_address");
  }

  // The bottom bit of the priority is used to hold if the address is 
  // a secondary address (e.g. with IPv6 privacy extensions) or not;
  // secondary addresses have lower priority (higher number).
  int best_addr_pri = get_priority(best_addr) * 2;

  // Get all the addresses via Linux' netlink interface.
  int fd = ::socket(PF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
  if (fd == -1) {
    return false;
  }

  struct sockaddr_nl nladdr;
  memset(&nladdr, 0, sizeof(nladdr));
  nladdr.nl_family = AF_NETLINK;
  if (::bind(fd, (sockaddr *)&nladdr, sizeof(nladdr))) {
    ::close(fd);
    return false;
  }

  const int seq_no = 1;
  struct {
    nlmsghdr nh;
    rtgenmsg g;
  } req;
  memset(&req, 0, sizeof(req));

  req.nh.nlmsg_len = sizeof(req);
  req.nh.nlmsg_type = RTM_GETADDR;
  req.nh.nlmsg_flags = NLM_F_ROOT | NLM_F_MATCH | NLM_F_REQUEST;
  req.nh.nlmsg_pid = getpid();
  req.nh.nlmsg_seq = seq_no;
  req.g.rtgen_family = AF_UNSPEC;

  int ret;
  do {
    ret = ::sendto(fd, &req, sizeof(req), 0, (sockaddr *)&nladdr, sizeof(nladdr));
  } while (ret == -1 && errno == EINTR);

  if (ret == -1) {
    ::close(fd);
    return false;
  }

  bool done = false;
  do {
    char buf[4096];
    socklen_t len = sizeof(nladdr);
    do {
      ret = ::recvfrom(fd, buf, sizeof(buf), 0, (sockaddr *)&nladdr, &len);
    } while (ret == -1 && errno == EINTR);

    if (ret < 0) {
      ::close(fd);
      return false;
    }

    for (const nlmsghdr *nlmsg = (const nlmsghdr *)buf;
         NLMSG_OK(nlmsg, ret);
         nlmsg = NLMSG_NEXT(nlmsg, ret)) {
      if (nlmsg->nlmsg_seq != seq_no)
        continue;
      if (nlmsg->nlmsg_type == NLMSG_DONE) {
        done = true;
        break;
      }
      if (nlmsg->nlmsg_type == NLMSG_ERROR) {
        ::close(fd);
        return false;
      }
      if (nlmsg->nlmsg_type != RTM_NEWADDR)
        continue;

      const ifaddrmsg *ifa = (const ifaddrmsg *)NLMSG_DATA(nlmsg);

      if (ifa->ifa_family != family)
        continue; 

#ifdef IFA_F_OPTIMISTIC
      if ((ifa->ifa_flags & IFA_F_OPTIMISTIC) != 0)
        continue;
#endif
#ifdef IFA_F_DADFAILED
      if ((ifa->ifa_flags & IFA_F_DADFAILED) != 0)
        continue;
#endif
#ifdef IFA_F_DEPRECATED
      if ((ifa->ifa_flags & IFA_F_DEPRECATED) != 0)
        continue;
#endif
#ifdef IFA_F_TENTATIVE
      if ((ifa->ifa_flags & IFA_F_TENTATIVE) != 0)
        continue;
#endif
  
      // Since there can be point-to-point links on the machine, we need to keep
      // track of the addresses we've seen for this interface; if we see both
      // IFA_LOCAL and IFA_ADDRESS for an interface, keep only the IFA_LOCAL.
      rak::socket_address this_addr;
      bool seen_addr = false;
      int plen = IFA_PAYLOAD(nlmsg);
      for (const rtattr *rta = IFA_RTA(ifa);
           RTA_OK(rta, plen);
	   rta = RTA_NEXT(rta, plen)) {
        if (rta->rta_type != IFA_LOCAL &&
            rta->rta_type != IFA_ADDRESS) {
          continue;
        }
        if (rta->rta_type == IFA_ADDRESS && seen_addr) {
          continue;
        }
        seen_addr = true;
        switch (ifa->ifa_family) {
        case AF_INET:
          this_addr.sa_inet()->clear();
          this_addr.sa_inet()->set_address(*(const in_addr *)RTA_DATA(rta));
          break;
        case AF_INET6:
          this_addr.sa_inet6()->clear();
          this_addr.sa_inet6()->set_address(*(const in6_addr *)RTA_DATA(rta));
          break;
        }
      }
      if (!seen_addr)
        continue;
       
      int this_addr_pri = get_priority(this_addr) * 2;
      if ((ifa->ifa_flags & IFA_F_SECONDARY) == IFA_F_SECONDARY) {
        ++this_addr_pri;
      }

      if (this_addr_pri < best_addr_pri) {
        best_addr = this_addr;
        best_addr_pri = this_addr_pri;
      }
    }
  } while (!done);

  ::close(fd);
  if (!best_addr.is_address_any()) {
    *address = best_addr;
    return true;
  } else {
    return false;
  } 
}

#else

// Generic POSIX variant.
bool
get_local_address(sa_family_t family, rak::socket_address *address) {
  SocketFd sock;
  if (!sock.open_datagram()) {
    return false;
  }

  rak::socket_address dummy_dest;
  dummy_dest.clear();

  switch (family) {
  case rak::socket_address::af_inet:
    dummy_dest.set_address_c_str("4.0.0.0"); 
    break;
  case rak::socket_address::af_inet6:
    dummy_dest.set_address_c_str("2001:700::"); 
    break;
  default:
    throw internal_error("Unknown address family");
  }

  dummy_dest.set_port(80);

  if (!sock.connect(dummy_dest)) {
    sock.close();
    return false;
  }

  bool ret = sock.getsockname(address);
  sock.close();

  return ret;
}

#endif

}
