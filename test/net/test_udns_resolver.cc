#include "config.h"

#include "test/net/test_udns_resolver.h"

#include <arpa/inet.h>
#include <cstdint>
#include <cstdlib>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

#include "net/udns_library.h"

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(test_udns_resolver, "net");

namespace {

// A UDP socket on the loopback interface standing in for the configured
// nameserver, so that no query leaves the host.
class scratch_server {
public:
  scratch_server() {
    m_fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    CPPUNIT_ASSERT(m_fd >= 0);

    sockaddr_in sa = {};
    sa.sin_family      = AF_INET;
    sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    CPPUNIT_ASSERT(::bind(m_fd, reinterpret_cast<sockaddr*>(&sa), sizeof(sa)) == 0);

    socklen_t sa_length = sizeof(sa);
    CPPUNIT_ASSERT(::getsockname(m_fd, reinterpret_cast<sockaddr*>(&sa), &sa_length) == 0);

    m_port = ntohs(sa.sin_port);
  }

  ~scratch_server() {
    if (m_fd >= 0)
      ::close(m_fd);
  }

  scratch_server(const scratch_server&) = delete;
  scratch_server& operator=(const scratch_server&) = delete;

  int      fd() const   { return m_fd; }
  uint16_t port() const { return m_port; }

private:
  int      m_fd{-1};
  uint16_t m_port{0};
};

struct callback_state {
  int      call_count{0};
  int      record_count{0};
  uint32_t address{0};
};

void
callback_a4(struct dns_ctx*, struct dns_rr_a4* result, void* data) {
  auto state = static_cast<callback_state*>(data);

  state->call_count++;

  if (result == nullptr)
    return;

  state->record_count = result->dnsa4_nrr;

  if (result->dnsa4_nrr > 0)
    state->address = result->dnsa4_addr[0].s_addr;

  std::free(result);
}

struct dns_ctx*
open_context(uint16_t port) {
  // Reset the default context rather than calling dns_init(), which would read
  // the host resolver configuration and add real nameservers.
  dns_reset(nullptr);

  auto ctx = dns_new(nullptr);
  CPPUNIT_ASSERT(ctx != nullptr);

  CPPUNIT_ASSERT(dns_add_serv(ctx, "127.0.0.1") == 1);
  CPPUNIT_ASSERT(dns_set_opt(ctx, DNS_OPT_PORT, port) >= 0);
  CPPUNIT_ASSERT(dns_open(ctx) >= 0);

  return ctx;
}

// dns_submit_a4() only queues the query; dns_timeouts() writes it to the socket.
void
submit_query(struct dns_ctx* ctx, const char* name, callback_state* state) {
  CPPUNIT_ASSERT(dns_submit_a4(ctx, name, DNS_NOSRCH, callback_a4, state) != nullptr);
  CPPUNIT_ASSERT(dns_timeouts(ctx, 0, 0) >= 0);
}

bool
wait_readable(int fd) {
  pollfd descriptor = {fd, POLLIN, 0};

  return ::poll(&descriptor, 1, 5000) == 1;
}

std::vector<uint8_t>
receive_query(const scratch_server& server, sockaddr_in* from) {
  CPPUNIT_ASSERT(wait_readable(server.fd()));

  std::vector<uint8_t> buffer(512);
  socklen_t            from_length = sizeof(*from);

  auto size = ::recvfrom(server.fd(), buffer.data(), buffer.size(), 0,
                         reinterpret_cast<sockaddr*>(from), &from_length);

  CPPUNIT_ASSERT(size > DNS_HSIZE);
  buffer.resize(size);

  return buffer;
}

// Rebuild the packet as the queried nameserver would answer it: the question
// section, then a single A record. The EDNS0 record the query carries in its
// additional section is dropped. Only the QR flag is under test.
std::vector<uint8_t>
build_answer(const std::vector<uint8_t>& query, bool set_qr_flag) {
  size_t offset = DNS_HSIZE;

  while (offset < query.size() && query[offset] != 0)
    offset += query[offset] + 1;

  offset += 1 + 4;
  CPPUNIT_ASSERT(offset <= query.size());

  std::vector<uint8_t> answer(query.begin(), query.begin() + offset);

  if (set_qr_flag)
    answer[DNS_H_F1] |= DNS_HF1_QR;

  answer[DNS_H_ANCNT1] = 0;
  answer[DNS_H_ANCNT2] = 1;
  answer[DNS_H_NSCNT1] = 0;
  answer[DNS_H_NSCNT2] = 0;
  answer[DNS_H_ARCNT1] = 0;
  answer[DNS_H_ARCNT2] = 0;

  const uint8_t record[] = {
    0xc0, DNS_HSIZE,          // name: pointer to the question name
    0x00, 0x01,               // type: A
    0x00, 0x01,               // class: IN
    0x00, 0x00, 0x00, 0x3c,   // ttl: 60
    0x00, 0x04,               // rdlength
    10, 1, 2, 3               // rdata
  };

  answer.insert(answer.end(), record, record + sizeof(record));

  return answer;
}

void
send_answer(const scratch_server& server, const sockaddr_in& to, const std::vector<uint8_t>& answer) {
  auto size = ::sendto(server.fd(), answer.data(), answer.size(), 0,
                       reinterpret_cast<const sockaddr*>(&to), sizeof(to));

  CPPUNIT_ASSERT(size == static_cast<ssize_t>(answer.size()));
}

} // namespace

void
test_udns_resolver::test_random32_exceeds_microsecond_range() {
  // A clock-derived value cannot exceed the largest tv_usec, so a single value
  // above that range shows the query id seed does not come from the clock.
  bool above_microsecond_range = false;

  for (int i = 0; i < 64 && !above_microsecond_range; i++)
    above_microsecond_range = udns_random32() > 999999;

  CPPUNIT_ASSERT(above_microsecond_range);
}

void
test_udns_resolver::test_reply_without_qr_flag_is_rejected() {
  scratch_server server;
  callback_state state;

  auto ctx = open_context(server.port());

  submit_query(ctx, "spoofed.test.invalid", &state);

  sockaddr_in from  = {};
  auto        query = receive_query(server, &from);

  send_answer(server, from, build_answer(query, false));

  CPPUNIT_ASSERT(wait_readable(dns_sock(ctx)));
  dns_ioevent(ctx, 0);

  CPPUNIT_ASSERT_EQUAL(0, state.call_count);
  CPPUNIT_ASSERT_EQUAL(1, dns_active(ctx));

  dns_free(ctx);
}

void
test_udns_resolver::test_reply_with_qr_flag_is_accepted() {
  scratch_server server;
  callback_state state;

  auto ctx = open_context(server.port());

  submit_query(ctx, "answered.test.invalid", &state);

  sockaddr_in from  = {};
  auto        query = receive_query(server, &from);

  send_answer(server, from, build_answer(query, true));

  CPPUNIT_ASSERT(wait_readable(dns_sock(ctx)));
  dns_ioevent(ctx, 0);

  CPPUNIT_ASSERT_EQUAL(1, state.call_count);
  CPPUNIT_ASSERT_EQUAL(1, state.record_count);
  CPPUNIT_ASSERT_EQUAL(inet_addr("10.1.2.3"), state.address);
  CPPUNIT_ASSERT_EQUAL(0, dns_active(ctx));

  dns_free(ctx);
}
