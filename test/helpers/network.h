#ifndef LIBTORRENT_HELPER_NETWORK_H
#define LIBTORRENT_HELPER_NETWORK_H

#include <functional>
#include <string>
#include <cppunit/extensions/HelperMacros.h>

#include "torrent/net/address_info.h"

//
// Socket addresses
//

#define TEST_DEFAULT_SA                                                 \
  auto sin_any = wrap_ai_get_first_sa("0.0.0.0");                       \
  auto sin_any_5000 = wrap_ai_get_first_sa("0.0.0.0", "5000");          \
  auto sin_any_5005 = wrap_ai_get_first_sa("0.0.0.0", "5005");          \
  auto sin_bc = wrap_ai_get_first_sa("255.255.255.255");                \
  auto sin_bc_5000 = wrap_ai_get_first_sa("255.255.255.255", "5000");   \
  auto sin_bnd = wrap_ai_get_first_sa("123.123.123.123");               \
  auto sin_1 = wrap_ai_get_first_sa("1.2.3.4");                         \
  auto sin_1_5000 = wrap_ai_get_first_sa("1.2.3.4", "5000");            \
  auto sin_1_5005 = wrap_ai_get_first_sa("1.2.3.4", "5005");            \
  auto sin_1_5100 = wrap_ai_get_first_sa("1.2.3.4", "5100");            \
  auto sin_2      = wrap_ai_get_first_sa("4.3.2.1");                    \
  auto sin_2_5000 = wrap_ai_get_first_sa("4.3.2.1", "5000");            \
  auto sin_2_5100 = wrap_ai_get_first_sa("4.3.2.1", "5100");            \
                                                                        \
  auto sin6_any = wrap_ai_get_first_sa("::");                           \
  auto sin6_any_5000 = wrap_ai_get_first_sa("::", "5000");              \
  auto sin6_any_5005 = wrap_ai_get_first_sa("::", "5005");              \
  auto sin6_bnd = wrap_ai_get_first_sa("ff01::123");                    \
  auto sin6_1 = wrap_ai_get_first_sa("ff01::1");                        \
  auto sin6_1_5000 = wrap_ai_get_first_sa("ff01::1", "5000");           \
  auto sin6_1_5005 = wrap_ai_get_first_sa("ff01::1", "5005");           \
  auto sin6_1_5100 = wrap_ai_get_first_sa("ff01::1", "5100");           \
  auto sin6_2      = wrap_ai_get_first_sa("ff02::2");                   \
  auto sin6_2_5000 = wrap_ai_get_first_sa("ff02::2", "5000");           \
  auto sin6_2_5100 = wrap_ai_get_first_sa("ff02::2", "5100");           \
  auto sin6_v4_1 = wrap_ai_get_first_sa("::ffff:1.2.3.4");              \
  auto sin6_v4_1_5000 = wrap_ai_get_first_sa("::ffff:1.2.3.4", "5000"); \
  auto sin6_v4_any = wrap_ai_get_first_sa("::ffff:0.0.0.0");            \
  auto sin6_v4_any_5000 = wrap_ai_get_first_sa("::ffff:0.0.0.0", "5000"); \
  auto sin6_v4_bc = wrap_ai_get_first_sa("::ffff:255.255.255.255");     \
  auto sin6_v4_bc_5000 = wrap_ai_get_first_sa("::ffff:255.255.255.255", "5000"); \
  auto sin6_v4_bnd = wrap_ai_get_first_sa("::ffff:123.123.123.123");    \
                                                                        \
  auto c_sin_any = wrap_ai_get_first_c_sa("0.0.0.0");                   \
  auto c_sin_any_5000 = wrap_ai_get_first_c_sa("0.0.0.0", "5000");      \
  auto c_sin_any_5005 = wrap_ai_get_first_c_sa("0.0.0.0", "5005");      \
  auto c_sin_any_5010 = wrap_ai_get_first_c_sa("0.0.0.0", "5010");      \
  auto c_sin_any_6881 = wrap_ai_get_first_c_sa("0.0.0.0", "6881");      \
  auto c_sin_any_6900 = wrap_ai_get_first_c_sa("0.0.0.0", "6900");      \
  auto c_sin_any_6999 = wrap_ai_get_first_c_sa("0.0.0.0", "6999");      \
  auto c_sin_bc = wrap_ai_get_first_c_sa("255.255.255.255");            \
  auto c_sin_bc_5000 = wrap_ai_get_first_c_sa("255.255.255.255", "5000"); \
  auto c_sin_bnd = wrap_ai_get_first_c_sa("123.123.123.123");           \
  auto c_sin_bnd_5000 = wrap_ai_get_first_c_sa("123.123.123.123", "5000"); \
  auto c_sin_bnd_6881 = wrap_ai_get_first_c_sa("123.123.123.123", "6881"); \
  auto c_sin_bnd_6900 = wrap_ai_get_first_c_sa("123.123.123.123", "6900"); \
  auto c_sin_bnd_6999 = wrap_ai_get_first_c_sa("123.123.123.123", "6999"); \
  auto c_sin_1 = wrap_ai_get_first_c_sa("1.2.3.4");                     \
  auto c_sin_1_5000 = wrap_ai_get_first_c_sa("1.2.3.4", "5000");        \
  auto c_sin_1_5005 = wrap_ai_get_first_c_sa("1.2.3.4", "5005");        \
  auto c_sin_1_5010 = wrap_ai_get_first_c_sa("1.2.3.4", "5010");        \
  auto c_sin_1_6881 = wrap_ai_get_first_c_sa("1.2.3.4", "6881");        \
  auto c_sin_1_6900 = wrap_ai_get_first_c_sa("1.2.3.4", "6900");        \
  auto c_sin_1_6999 = wrap_ai_get_first_c_sa("1.2.3.4", "6999");        \
  auto c_sin_2      = wrap_ai_get_first_c_sa("4.3.2.1");                \
  auto c_sin_2_5000 = wrap_ai_get_first_c_sa("4.3.2.1", "5000");        \
  auto c_sin_2_5100 = wrap_ai_get_first_c_sa("4.3.2.1", "5100");        \
                                                                        \
  auto c_sin6_any = wrap_ai_get_first_c_sa("::");                       \
  auto c_sin6_any_5000 = wrap_ai_get_first_c_sa("::", "5000");          \
  auto c_sin6_any_5005 = wrap_ai_get_first_c_sa("::", "5005");          \
  auto c_sin6_any_5010 = wrap_ai_get_first_c_sa("::", "5010");          \
  auto c_sin6_any_6881 = wrap_ai_get_first_c_sa("::", "6881");          \
  auto c_sin6_any_6900 = wrap_ai_get_first_c_sa("::", "6900");          \
  auto c_sin6_any_6999 = wrap_ai_get_first_c_sa("::", "6999");          \
  auto c_sin6_bnd = wrap_ai_get_first_c_sa("ff01::123");                \
  auto c_sin6_bnd_5000 = wrap_ai_get_first_c_sa("ff01::123", "5000");   \
  auto c_sin6_bnd_6881 = wrap_ai_get_first_c_sa("ff01::123", "6881");   \
  auto c_sin6_bnd_6900 = wrap_ai_get_first_c_sa("ff01::123", "6900");   \
  auto c_sin6_bnd_6999 = wrap_ai_get_first_c_sa("ff01::123", "6999");   \
  auto c_sin6_v4_1_5000 = wrap_ai_get_first_c_sa("::ffff:1.2.3.4", "5000"); \
  auto c_sin6_1 = wrap_ai_get_first_c_sa("ff01::1");                    \
  auto c_sin6_1_5000 = wrap_ai_get_first_c_sa("ff01::1", "5000");       \
  auto c_sin6_1_5005 = wrap_ai_get_first_c_sa("ff01::1", "5005");       \
  auto c_sin6_1_5010 = wrap_ai_get_first_c_sa("ff01::1", "5010");       \
  auto c_sin6_1_5100 = wrap_ai_get_first_c_sa("ff01::1", "5100");       \
  auto c_sin6_1_6881 = wrap_ai_get_first_c_sa("ff01::1", "6881");       \
  auto c_sin6_1_6900 = wrap_ai_get_first_c_sa("ff01::1", "6900");       \
  auto c_sin6_1_6999 = wrap_ai_get_first_c_sa("ff01::1", "6999");       \
  auto c_sin6_2      = wrap_ai_get_first_c_sa("ff02::2");               \
  auto c_sin6_2_5000 = wrap_ai_get_first_c_sa("ff02::2", "5000");       \
  auto c_sin6_2_5100 = wrap_ai_get_first_c_sa("ff02::2", "5100");

inline bool
compare_sin6_addr(in6_addr lhs, in6_addr rhs) {
  return std::equal(lhs.s6_addr, lhs.s6_addr + 16, rhs.s6_addr);
}

inline bool
compare_listen_result(const torrent::listen_result_type& lhs, int rhs_fd, const torrent::c_sa_unique_ptr& rhs_sap) {
  return lhs.fd == rhs_fd &&
    ((lhs.address && rhs_sap) || ((lhs.address && rhs_sap) && torrent::sap_equal(lhs.address, rhs_sap)));
}

inline torrent::sa_unique_ptr
wrap_ai_get_first_sa(const char* nodename, const char* servname = nullptr, const addrinfo* hints = nullptr) {
  auto sa = torrent::ai_get_first_sa(nodename, servname, hints);

  CPPUNIT_ASSERT_MESSAGE(("wrap_ai_get_first_sa: nodename:'" + std::string(nodename) + "'").c_str(),
                        sa != nullptr);

  return sa;
}

inline torrent::c_sa_unique_ptr
wrap_ai_get_first_c_sa(const char* nodename, const char* servname = nullptr, const addrinfo* hints = nullptr) {
  auto sa = torrent::ai_get_first_sa(nodename, servname, hints);

  CPPUNIT_ASSERT_MESSAGE(("wrap_ai_get_first_sa: nodename:'" + std::string(nodename) + "'").c_str(),
                        sa != nullptr);

  return torrent::c_sa_unique_ptr(sa.release());
}

//
// Address info tests:
//

typedef std::function<int (torrent::ai_unique_ptr&)> test_ai_ref;

enum ai_flags_enum : int {
      aif_none = 0x0,
      aif_inet = 0x1,
      aif_inet6 = 0x2,
      aif_any = 0x4,
};

constexpr ai_flags_enum operator | (ai_flags_enum a, ai_flags_enum b) {
  return static_cast<ai_flags_enum>(static_cast<int>(a) | static_cast<int>(b));
}

template <ai_flags_enum ai_flags>
inline bool
test_valid_ai_ref(test_ai_ref ftor, uint16_t port = 0) {
  torrent::ai_unique_ptr ai;

  if (int err = ftor(ai)) {
    std::cout << std::endl << "valid_ai_ref got error '" << gai_strerror(err) << "'" << std::endl;
    return false;
  }

  if ((ai_flags & aif_inet) && !torrent::sa_is_inet(ai->ai_addr))
    return false;

  if ((ai_flags & aif_inet6) && !torrent::sa_is_inet6(ai->ai_addr))
    return false;

  if (!!(ai_flags & aif_any) == !torrent::sa_is_any(ai->ai_addr))
    return false;

  if (torrent::sa_port(ai->ai_addr) != port)
    return false;

  return true;
}

inline bool
test_valid_ai_ref_err(test_ai_ref ftor, int expect_err) {
  torrent::ai_unique_ptr ai;
  int err = ftor(ai);

  if (err != expect_err) {
    std::cout << std::endl << "ai_ref_err got wrong error, expected '" << gai_strerror(expect_err) << "', got '" << gai_strerror(err) << "'" << std::endl;
    return false;
  }

  return true;
}

#endif
