#include "config.h"

#include "test_file_manager.h"

#include <cstdlib>
#include <memory>
#include <string>
#include <vector>
#include <unistd.h>

#include "data/memory_chunk.h"
#include "data/socket_file.h"
#include "torrent/data/file.h"
#include "torrent/data/file_manager.h"

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(test_file_manager, "data");

namespace {

// File::set_frozen_path is protected and FileList is its only friend, so point the
// files at real paths through a derived type instead of relaxing File.
class path_file : public torrent::File {
public:
  void set_test_path(const std::string& path) { set_frozen_path(path); }
};

class file_manager_fixture {
public:
  file_manager_fixture(unsigned int count, torrent::FileManager::size_type max_open_files);
  ~file_manager_fixture();

  torrent::FileManager& manager()          { return m_manager; }
  torrent::File*        file(size_t index) { return m_files[index].get(); }

private:
  file_manager_fixture(const file_manager_fixture&) = delete;
  file_manager_fixture& operator=(const file_manager_fixture&) = delete;

  std::string path_for(size_t index) const { return m_directory + "/file_" + std::to_string(index); }

  std::string                             m_directory{"/tmp/lt_test_file_manager_XXXXXX"};
  std::vector<std::unique_ptr<path_file>> m_files;
  torrent::FileManager                    m_manager;
};

file_manager_fixture::file_manager_fixture(unsigned int count, torrent::FileManager::size_type max_open_files) {
  CPPUNIT_ASSERT(::mkdtemp(m_directory.data()) != nullptr);

  m_manager.set_max_open_files(max_open_files);

  for (unsigned int i = 0; i < count; i++) {
    auto file = std::make_unique<path_file>();

    file->set_test_path(path_for(i));

    CPPUNIT_ASSERT(m_manager.open(file.get(),
                                  false,
                                  torrent::MemoryChunk::prot_read | torrent::MemoryChunk::prot_write,
                                  torrent::SocketFile::o_create));

    // Eviction orders on last touched only, so make the order explicit.
    file->set_last_touched(100 + i * 10);

    m_files.push_back(std::move(file));
  }
}

file_manager_fixture::~file_manager_fixture() {
  for (auto& file : m_files)
    if (file->is_open())
      m_manager.close(file.get());

  for (size_t i = 0; i < m_files.size(); i++)
    ::unlink(path_for(i).c_str());

  ::rmdir(m_directory.c_str());
}

} // namespace

void
test_file_manager::test_evict_closes_requested_count() {
  file_manager_fixture fixture(48, 64);

  // Lower the limit by one, so exactly one file has to be evicted.
  fixture.manager().set_max_open_files(47);

  CPPUNIT_ASSERT_EQUAL(static_cast<uint64_t>(1), fixture.manager().files_closed_counter());
  CPPUNIT_ASSERT_EQUAL(static_cast<torrent::FileManager::size_type>(47), fixture.manager().open_files());

  CPPUNIT_ASSERT(!fixture.file(0)->is_open());
  CPPUNIT_ASSERT(fixture.file(1)->is_open());
  CPPUNIT_ASSERT(fixture.file(2)->is_open());
}

void
test_file_manager::test_evict_uses_least_active_cache() {
  file_manager_fixture fixture(48, 64);

  // Closes file 0 and caches files 1 and 2 as the next eviction candidates.
  fixture.manager().set_max_open_files(47);

  CPPUNIT_ASSERT_EQUAL(static_cast<uint64_t>(1), fixture.manager().files_closed_counter());
  CPPUNIT_ASSERT(fixture.file(1)->is_open());
  CPPUNIT_ASSERT(fixture.file(2)->is_open());

  // Make file 3 the least active file. A fresh scan would now pick it over file 1,
  // while the cached entry for file 1 stays valid as its last touched is unchanged.
  fixture.file(3)->set_last_touched(1);

  fixture.manager().set_max_open_files(46);

  // The cache was used: file 1 got closed and file 3, which a scan would have
  // picked, is still open.
  CPPUNIT_ASSERT(!fixture.file(1)->is_open());
  CPPUNIT_ASSERT(fixture.file(3)->is_open());
  CPPUNIT_ASSERT(fixture.file(2)->is_open());

  CPPUNIT_ASSERT_EQUAL(static_cast<uint64_t>(2), fixture.manager().files_closed_counter());
  CPPUNIT_ASSERT_EQUAL(static_cast<torrent::FileManager::size_type>(46), fixture.manager().open_files());
}
