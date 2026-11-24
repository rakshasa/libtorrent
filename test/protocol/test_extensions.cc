#include "config.h"

#include "test/protocol/test_extensions.h"

#include <memory>

#include "download/download_main.h"
#include "protocol/extensions.h"
#include "protocol/peer_connection_base.h"
#include "protocol/peer_connection_state.h"
#include "torrent/download_info.h"

CPPUNIT_TEST_SUITE_REGISTRATION(TestExtensions);

namespace {

class extension_connection : public torrent::PeerConnectionBase {
public:
  extension_connection() { m_extensions = new torrent::ProtocolExtension; }

  void initialize_custom() override {}
  void update_interested() override {}
  bool receive_keepalive() override { return true; }
  void event_read() override {}
  void event_write() override {}

  using PeerConnectionBase::send_pending_extension;

  void schedule_pex() { m_up.schedule_pex_enable(); }
  void schedule_pex_exchange() { m_up.schedule_pex_exchange(); }
  bool has_pending_pex() const { return m_up.has_pending_pex(); }
  auto* buffer() { return m_up.protocol->buffer(); }
  void flush_extension() { m_extension_message.clear(); buffer()->reset(); }
};

} // namespace

void
TestExtensions::test_pex_count_released_on_destroy() {
  auto download = std::make_unique<torrent::DownloadMain>();

  auto* extensions = new torrent::ProtocolExtension;
  extensions->set_info(nullptr, download.get());

  CPPUNIT_ASSERT_EQUAL(uint32_t{0}, download->info()->size_pex());

  extensions->set_local_enabled(torrent::ProtocolExtension::UT_PEX);
  CPPUNIT_ASSERT_EQUAL(uint32_t{1}, download->info()->size_pex());

  // Torn down without cleanup(), as on the connection-setup failure
  // paths. The PEX count on the download must still be released.
  delete extensions;

  CPPUNIT_ASSERT_EQUAL(uint32_t{0}, download->info()->size_pex());
}

void
TestExtensions::test_pex_count_not_released_twice() {
  auto download = std::make_unique<torrent::DownloadMain>();

  auto* extensions = new torrent::ProtocolExtension;
  extensions->set_info(nullptr, download.get());

  extensions->set_local_enabled(torrent::ProtocolExtension::UT_PEX);
  extensions->cleanup();
  CPPUNIT_ASSERT_EQUAL(uint32_t{0}, download->info()->size_pex());

  // Destruction after a proper cleanup must not decrement again.
  delete extensions;

  CPPUNIT_ASSERT_EQUAL(uint32_t{0}, download->info()->size_pex());
}

void
TestExtensions::test_pending_pex_transitions() {
  torrent::protocol::StateUp up;

  up.schedule_pex_exchange();
  up.schedule_pex_enable();
  up.schedule_pex_disable();
  CPPUNIT_ASSERT(up.has_pending_pex_toggle());
  CPPUNIT_ASSERT(!up.has_pending_pex_enable());
  CPPUNIT_ASSERT(up.has_pending_pex_exchange());

  up.schedule_pex_enable();
  CPPUNIT_ASSERT(up.has_pending_pex_enable());
  CPPUNIT_ASSERT(up.has_pending_pex_exchange());

  up.clear_pending_pex_toggle();
  CPPUNIT_ASSERT(!up.has_pending_pex_toggle());
  CPPUNIT_ASSERT(up.has_pending_pex());

  up.schedule_pex_disable();
  up.clear_pending_pex_exchange();
  CPPUNIT_ASSERT(up.has_pending_pex_toggle());
  CPPUNIT_ASSERT(!up.has_pending_pex_exchange());

  up.clear_pending_pex();
  CPPUNIT_ASSERT(!up.has_pending_pex());
}

void
TestExtensions::test_pending_extension_priority() {
  extension_connection connection;
  auto* extensions = connection.extensions();
  torrent::Piece piece(0, 0, torrent::ProtocolExtension::metadata_piece_size);

  extensions->set_remote_supported(torrent::ProtocolExtension::UT_PEX);
  CPPUNIT_ASSERT(extensions->request_metadata_piece(&piece));
  connection.schedule_pex();

  auto* buffer = connection.buffer();
  buffer->set_end(buffer->reserved() - torrent::ProtocolBase::sizeof_extension + 1);
  CPPUNIT_ASSERT(!connection.send_pending_extension());
  CPPUNIT_ASSERT(connection.has_pending_pex());
  CPPUNIT_ASSERT(extensions->has_pending_message());

  buffer->reset();
  CPPUNIT_ASSERT(connection.send_pending_extension());
  CPPUNIT_ASSERT(!connection.has_pending_pex());
  CPPUNIT_ASSERT(extensions->has_pending_message());

  connection.flush_extension();
  CPPUNIT_ASSERT(connection.send_pending_extension());
  CPPUNIT_ASSERT(!extensions->has_pending_message());
}

void
TestExtensions::test_pending_extension_fallback() {
  extension_connection connection;
  auto* extensions = connection.extensions();
  torrent::Piece piece(0, 0, torrent::ProtocolExtension::metadata_piece_size);

  CPPUNIT_ASSERT(!connection.send_pending_extension());
  CPPUNIT_ASSERT(extensions->request_metadata_piece(&piece));
  connection.schedule_pex();

  CPPUNIT_ASSERT(connection.send_pending_extension());
  CPPUNIT_ASSERT(!connection.has_pending_pex());
  CPPUNIT_ASSERT(!extensions->has_pending_message());
}

void
TestExtensions::test_pending_extension_pex_off() {
  extension_connection connection;
  auto* extensions = connection.extensions();
  torrent::Piece piece(0, 0, torrent::ProtocolExtension::metadata_piece_size);

  // The peer announced ut_pex with id 0, as rtorrent does while PEX is off for the torrent.
  extensions->set_remote_supported(torrent::ProtocolExtension::UT_PEX);
  CPPUNIT_ASSERT_EQUAL(uint8_t{0}, extensions->id(torrent::ProtocolExtension::UT_PEX));

  // Nothing is written for the peer exchange, so the caller may go on to the pieces.
  connection.schedule_pex_exchange();
  CPPUNIT_ASSERT(!connection.send_pending_extension());
  CPPUNIT_ASSERT(!connection.has_pending_pex());
  CPPUNIT_ASSERT(connection.buffer()->size_end() == 0);

  // A pending extension message goes out in the same call.
  CPPUNIT_ASSERT(extensions->request_metadata_piece(&piece));
  connection.schedule_pex_exchange();
  CPPUNIT_ASSERT(connection.send_pending_extension());
  CPPUNIT_ASSERT(!connection.has_pending_pex());
  CPPUNIT_ASSERT(!extensions->has_pending_message());
}
