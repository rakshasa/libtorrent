#include "config.h"

#include "test_torrent.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <utility>
#include <sys/resource.h>
#include <unistd.h>

#include "torrent/data/file.h"
#include "torrent/data/file_list.h"
#include "torrent/download.h"
#include "torrent/download_info.h"
#include "torrent/exceptions.h"
#include "torrent/object.h"
#include "torrent/path.h"
#include "torrent/runtime/client_config.h"
#include "torrent/torrent.h"
#include "torrent/utils/functional.h"

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(test_torrent, "torrent");

namespace {

torrent::Object*
create_metafile(const std::string& name, int64_t length) {
  auto object = new torrent::Object(torrent::Object::create_map());
  auto& info = object->insert_key("info", torrent::Object::create_map());
  info.insert_key("name", name);
  info.insert_key("length", length);
  info.insert_key("piece length", int64_t{16384});
  info.insert_key("pieces", std::string(20, '\x01'));
  return object;
}

torrent::Object*
create_magnet(const std::string& uri) {
  auto object = new torrent::Object(torrent::Object::create_map());
  object->insert_key("magnet-uri", uri);
  return object;
}

// Removes the download when the scope ends, so a failed assertion does not leave it registered.
struct scoped_download {
  ~scoped_download() {
    if (download.is_valid())
      torrent::download_remove(download);
  }

  torrent::Download download;
};

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
test_torrent::test_cleanup_removes_downloads() {
  char root_dir[] = "/tmp/libtorrent_test_torrent_XXXXXX";
  CPPUNIT_ASSERT(::mkdtemp(root_dir) != nullptr);

  const std::string data_path = std::string(root_dir) + "/data";
  std::ofstream(data_path) << "data";

  torrent::download_add(create_magnet("magnet:?xt=urn:btih:" + std::string(40, '1')), 0);
  torrent::download_add(create_metafile("closed", 1), 0);

  // An open download whose chunk is queued for hashing on the disk thread.
  auto download = torrent::download_add(create_metafile("data", 4), 0);
  download.file_list()->set_root_dir(root_dir);
  download.open();
  download.hash_check(false);
  CPPUNIT_ASSERT(torrent::main_thread::hash_queue_size() == 1);

  // The downloads are still registered, so cleanup() has to remove them itself.
  CPPUNIT_ASSERT_NO_THROW(torrent::cleanup());

  std::remove(data_path.c_str());
  ::rmdir(root_dir);

  // Bring the library up again for tearDown().
  torrent::initialize_main_thread();
  torrent::initialize();
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

void
test_torrent::test_download_find_unknown_hash() {
  auto hash = std::string(20, '\x33');

  scoped_download added{torrent::download_add(create_magnet("magnet:?xt=urn:btih:" + std::string(40, '3')), 0)};
  CPPUNIT_ASSERT(torrent::download_find(hash).ptr() == added.download.ptr());

  torrent::download_remove(added.download);
  added.download = torrent::Download();

  CPPUNIT_ASSERT(!torrent::download_find(hash).is_valid());
  CPPUNIT_ASSERT(!torrent::download_find(std::string(20, '\x44')).is_valid());
}

void
test_torrent::test_download_find_wrong_size() {
  scoped_download added{torrent::download_add(create_magnet("magnet:?xt=urn:btih:" + std::string(38, '3') + "00"), 0)};

  // The terminating null of a 19 byte string would complete this hash.
  CPPUNIT_ASSERT(!torrent::download_find(std::string(19, '\x33')).is_valid());
  CPPUNIT_ASSERT(!torrent::download_find(std::string(38, '3') + "00").is_valid());
  CPPUNIT_ASSERT(!torrent::download_find("").is_valid());
  // A longer string that starts with the registered hash.
  CPPUNIT_ASSERT(!torrent::download_find(std::string(19, '\x33') + std::string(1, '\0') + "x").is_valid());
}

void
test_torrent::test_single_file_name_utf8() {
  auto config = torrent::runtime::client_config();
  const bool previous_utf8 = config->file_name_allow_legacy_utf8();
  const bool previous_sanitized = config->torrent_name_use_sanitized();
  const auto previous_replace = config->file_name_replace_slash();
  auto restore = torrent::utils::scope_guard([=]() {
      config->set_file_name_allow_legacy_utf8(previous_utf8);
      config->set_torrent_name_use_sanitized(previous_sanitized);
      config->set_file_name_replace_slash(previous_replace);
    });

  const std::string legacy = "episode-\xF1\xE5\xF0\xE8\xFF.mkv";
  const std::string utf8 = "episode-\xD1\x81\xD0\xB5\xD1\x80\xD0\xB8\xD1\x8F.mkv";
  struct name_case {
    std::string name;
    std::string alias;
    std::string raw_path;
    std::string alias_path;
  };
  const name_case cases[] = {
    { legacy, utf8, legacy, utf8 },
    { "raw/episode.mkv", "utf8/episode.mkv", "raw_episode.mkv", "utf8_episode.mkv" },
    { "raw/episode.mkv", "", "raw_episode.mkv", "raw_episode.mkv" },
  };

  config->set_file_name_replace_slash("_");
  for (bool allow_utf8 : {false, true}) {
    config->set_file_name_allow_legacy_utf8(allow_utf8);
    for (bool use_sanitized : {false, true}) {
      config->set_torrent_name_use_sanitized(use_sanitized);
      for (const auto& entry : cases) {
        std::unique_ptr<torrent::Object> object(create_metafile(entry.name, 1));
        if (!entry.alias.empty())
          object->get_key("info").insert_key("name.utf-8", entry.alias);
        scoped_download added{torrent::download_add(object.get(), 0)};
        object.release();

        const auto path = added.download.file_list()->front()->path();
        const auto& expected = allow_utf8 && !entry.alias.empty() ? entry.alias_path : entry.raw_path;
        CPPUNIT_ASSERT(path->size() == 1);
        CPPUNIT_ASSERT_EQUAL(expected, path->front().str());
      }
    }
  }
}
