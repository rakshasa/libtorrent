#include "config.h"

#include "test_address_info.h"

#include lt_tr1_functional
#include "torrent/net/address_info.h"
#include "torrent/net/socket_address.h"

CPPUNIT_TEST_SUITE_REGISTRATION(test_address_info);

typedef std::function<int (torrent::ai_unique_ptr&)> test_ai_ref;

enum ai_flags_enum : int {
      aif_inet = 0x1,
      aif_inet6 = 0x2,
      aif_any = 0x4,
};

constexpr ai_flags_enum operator | (ai_flags_enum a, ai_flags_enum b) {
  return static_cast<ai_flags_enum>(static_cast<int>(a) | static_cast<int>(b));
}

template <ai_flags_enum ai_flags>
static bool
test_valid_ai_ref(test_ai_ref ftor, uint16_t port = 0) {
  torrent::ai_unique_ptr ai;

  if (int err = ftor(ai)) {
    std::cerr << std::endl << "valid_ai_ref got error '" << gai_strerror(err) << "'" << std::endl;
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

static bool
test_valid_ai_ref_err(test_ai_ref ftor, int expect_err) {
  torrent::ai_unique_ptr ai;
  int err = ftor(ai);
  
  if (err != expect_err) {
    std::cerr << std::endl << "ai_ref_err got wrong error, expected '" << gai_strerror(expect_err) << "', got '" << gai_strerror(err) << "'" << std::endl;
    return false;
  }

  return true;
}

void
test_address_info::test_basic() {
  CPPUNIT_ASSERT(test_valid_ai_ref<aif_inet|aif_any> (std::bind(torrent::ai_get_addrinfo, "0.0.0.0", nullptr, nullptr, std::placeholders::_1)));
  CPPUNIT_ASSERT(test_valid_ai_ref<aif_inet6|aif_any>(std::bind(torrent::ai_get_addrinfo, "::", nullptr, nullptr, std::placeholders::_1)));

  CPPUNIT_ASSERT(test_valid_ai_ref<aif_inet> (std::bind(torrent::ai_get_addrinfo, "1.1.1.1", nullptr, nullptr, std::placeholders::_1)));
  CPPUNIT_ASSERT(test_valid_ai_ref<aif_inet6>(std::bind(torrent::ai_get_addrinfo, "ff01::1", nullptr, nullptr, std::placeholders::_1)));
  CPPUNIT_ASSERT(test_valid_ai_ref<aif_inet6>(std::bind(torrent::ai_get_addrinfo, "2001:0db8:85a3:0000:0000:8a2e:0370:7334", nullptr, nullptr, std::placeholders::_1)));

  CPPUNIT_ASSERT(test_valid_ai_ref<aif_inet> (std::bind(torrent::ai_get_addrinfo, "1.1.1.1", "22123", nullptr, std::placeholders::_1), 22123));
  CPPUNIT_ASSERT(test_valid_ai_ref<aif_inet6>(std::bind(torrent::ai_get_addrinfo, "2001:db8:a::", "22123", nullptr, std::placeholders::_1), 22123));

  CPPUNIT_ASSERT(test_valid_ai_ref_err(std::bind(torrent::ai_get_addrinfo, "1.1.1.300", nullptr, nullptr, std::placeholders::_1), EAI_NONAME));
  CPPUNIT_ASSERT(test_valid_ai_ref_err(std::bind(torrent::ai_get_addrinfo, "2001:db8:a::22123", nullptr, nullptr, std::placeholders::_1), EAI_NONAME));
}

void
test_address_info::test_helpers() {
}
