#include "config.h"

#include "test/protocol/test_buffered_messages.h"

#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

#include "net/throttle_list.h"
#include "protocol/peer_connection_leech.h"

CPPUNIT_TEST_SUITE_REGISTRATION(test_buffered_messages);

namespace {

using torrent::Download;
using torrent::ProtocolBase;

// A seed's connection on one end of a socket pair. Nothing is ever sent
// from the other end, so every read gives 0 bytes.
class seed_connection : public torrent::PeerConnection<Download::CONNECTION_SEED> {
public:
  explicit seed_connection(int fd) {
    set_file_descriptor(fd);
    m_down.protocol->set_throttle(&m_throttle);
  }

  ~seed_connection() override { reset_file_descriptor(); }

  ProtocolBase* protocol() { return m_down.protocol.get(); }

private:
  torrent::ThrottleList m_throttle;
};

// A keep-alive is a whole message that needs nothing but the connection.
const char keep_alive[4] = {0, 0, 0, 0};

} // namespace

void
test_buffered_messages::setUp() {
  TestFixtureWithMainThread::setUp();
  CPPUNIT_ASSERT(::socketpair(AF_UNIX, SOCK_STREAM, 0, m_fds) == 0);
  CPPUNIT_ASSERT(::fcntl(m_fds[0], F_SETFL, ::fcntl(m_fds[0], F_GETFL) | O_NONBLOCK) == 0);
}

void
test_buffered_messages::tearDown() {
  ::close(m_fds[0]);
  ::close(m_fds[1]);
  TestFixtureWithMainThread::tearDown();
}

void
test_buffered_messages::test_empty_socket_empty_buffer() {
  seed_connection connection(m_fds[0]);

  connection.event_read();

  CPPUNIT_ASSERT(connection.protocol()->last_command() == ProtocolBase::NONE);
  CPPUNIT_ASSERT(connection.protocol()->buffer()->remaining() == 0);
}

void
test_buffered_messages::test_empty_socket_buffered_message() {
  seed_connection connection(m_fds[0]);

  // What the handshake read past the bitfield, e.g. the INTERESTED that
  // some clients send in the same packet as their bitfield.
  connection.down().push_unread(keep_alive, sizeof(keep_alive));
  connection.event_read();

  CPPUNIT_ASSERT(connection.protocol()->last_command() == ProtocolBase::KEEP_ALIVE);
  CPPUNIT_ASSERT(connection.protocol()->buffer()->remaining() == 0);
}

void
test_buffered_messages::test_empty_socket_partial_message() {
  seed_connection connection(m_fds[0]);

  // A whole message and the first two bytes of the next one.
  connection.down().push_unread(keep_alive, sizeof(keep_alive));
  connection.down().push_unread(keep_alive, 2);
  connection.event_read();

  CPPUNIT_ASSERT(connection.protocol()->last_command() == ProtocolBase::KEEP_ALIVE);
  CPPUNIT_ASSERT(connection.protocol()->buffer()->remaining() == 2);
}
