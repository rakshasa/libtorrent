#include "config.h"

#include "test_torrent.h"

#include <memory>
#include <utility>
#include <sys/resource.h>

#include "torrent/download.h"
#include "torrent/download_info.h"
#include "torrent/exceptions.h"
#include "torrent/object.h"
#include "torrent/torrent.h"

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(test_torrent, "torrent");

namespace {

torrent::Object*
create_magnet(const std::string& uri) {
  auto object = new torrent::Object(torrent::Object::create_map());
  object->insert_key("magnet-uri", uri);
  return object;
}

} // namespace

void
test_torrent::setUp() {
  test_fixture::setUp();
  mock_redirect_defaults();

  // torrent::initialize() needs an open file limit of at least 512.
  rlimit limit{};
  if (getrlimit(RLIMIT_NOFILE, &limit) == 0 && limit.rlim_cur < 1024 && limit.rlim_max >= 1024) {
    limit.rlim_cur = 1024;
    setrlimit(RLIMIT_NOFILE, &limit);
  }

  torrent::initialize_main_thread();
  torrent::initialize();
}

void
test_torrent::tearDown() {
  torrent::cleanup();

  test_fixture::tearDown();
}

void
test_torrent::test_download_add_zero_info_hash() {
  const std::string zero_magnets[] = {
    "magnet:?xt=urn:btih:" + std::string(40, '0'),
    "magnet:?xt=urn:btih:" + std::string(32, 'A'),
    // A tag runs to the next '=', so "&xt" is not an exact topic and the zero hash is kept.
    "magnet:?xt=urn:btih:" + std::string(40, '0') + "&&xt=urn:btih:" + std::string(40, '1'),
  };

  for (const auto& uri : zero_magnets) {
    std::unique_ptr<torrent::Object> object(create_magnet(uri));
    CPPUNIT_ASSERT_THROW_MESSAGE(uri, torrent::download_add(object.get(), 0), torrent::input_error);
  }

  std::unique_ptr<torrent::Object> meta_download(new torrent::Object(torrent::Object::create_map()));
  auto& info = meta_download->insert_key("info", torrent::Object::create_map());
  info.insert_key("meta_download", int64_t{1});
  info.insert_key("name", std::string("zero.meta"));
  info.insert_key("pieces", std::string(20, '\0'));

  CPPUNIT_ASSERT_THROW(torrent::download_add(meta_download.get(), 0), torrent::input_error);

  // Only an all-zero hash is refused.
  const std::pair<std::string, std::string> valid_magnets[] = {
    { "magnet:?xt=urn:btih:" + std::string(38, '0') + "01", std::string(19, '\0') + '\x01' },
    { "magnet:?xt=urn:btih:" + std::string(40, '1'), std::string(20, '\x11') },
  };

  for (const auto& [uri, hash] : valid_magnets) {
    torrent::Download download;

    CPPUNIT_ASSERT_NO_THROW_MESSAGE(uri, download = torrent::download_add(create_magnet(uri), 0));
    CPPUNIT_ASSERT_MESSAGE(uri, download.info()->hash().str() == hash);
    torrent::download_remove(download);
  }
}
