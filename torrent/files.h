#ifndef LIBTORRENT_FILES_H
#define LIBTORRENT_FILES_H

#include "bitfield.h"
#include "data/storage.h"

#include <string>
#include <list>
#include <vector>

namespace torrent {

// TODO: Just hack this up to use the new data/storage* stuff so it can be
// tested. Propably should rewrite sooner or later.

class bencode;

class Files {
public:
  typedef std::list<std::string> Path;
  typedef std::vector<std::string> HashVector;

  struct FileInfo {
    FileInfo() : m_size(0) {}
    FileInfo(Path& p, uint64_t s) : m_path(p), m_size(s) {}

    Path     m_path;
    uint64_t m_size;
  };

  typedef std::vector<FileInfo> FileInfos;

  Files();
  ~Files();

  void set(const bencode& b);

  void rootDir(const std::string& s);
  const std::string& rootDir() const { return m_rootDir; }

  void openAll();
  void resizeAll();
  void closeAll();

  unsigned int chunkCompleted() const { return m_completed; }
  uint64_t     doneSize() const       { return m_doneSize; }

  bool doneChunk(Storage::Chunk c, const std::string& hash);

  Storage& storage() { return m_storage; }

  const HashVector& hashes() const { return m_hashes; }
  const BitField& bitfield() const { return m_bitfield; }

private:
  void createDirs();
  static void makeDir(const std::string& dir);

  Storage           m_storage;

  FileInfos         m_files;
  HashVector        m_hashes;
  BitField          m_bitfield;

  uint64_t          m_doneSize;
  unsigned int      m_completed;

  std::string       m_rootDir;
};

} // namespace torrent

#endif // LIBTORRENT_FILES_H
