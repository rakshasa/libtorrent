#ifndef LIBTORRENT_FILES_H
#define LIBTORRENT_FILES_H

#include "chunk.h"
#include "bitfield.h"
#include "storage.h"
#include <list>
#include <vector>

namespace torrent {

// This class handles conversion from the continous blocks of data
// to and from the files on disk.

class bencode;

class Files {
public:
  typedef std::list<std::string> Path;

  struct File {
    File() : m_position(0), m_length(0), m_storage(NULL) {}

    Path m_path;
    uint64_t m_position;
    uint64_t m_length;
    Storage* m_storage;
  };

  typedef std::vector<File> FileVector;
  typedef std::vector<std::string> HashVector;

  Files();
  ~Files();

  void set(const bencode& b);

  void rootDir(const std::string& s);
  const std::string& rootDir() const { return m_rootDir; }

  void openAll();
  void closeAll();
  void resizeAll();

  // Check hash and truncate files if they are too large.
  void checkHash();

  // This is ugly...
  unsigned int chunkSize() const { return m_chunkSize; }
  unsigned int chunkSize(unsigned int i) const {
    return i == chunkCount() - 1 && m_totalSize % m_chunkSize
      ? m_totalSize % m_chunkSize : m_chunkSize; }

  unsigned int chunkCount() const { return m_hashes.size(); }

  unsigned int chunkCompleted() const { return m_completed; }
  
  uint64_t doneSize() const { return m_doneSize; }
  uint64_t totalSize() const { return m_totalSize; }

  // allowEmpty returns a chunk with no Part's if the files were to short to contain
  // the chunk. getChunk resizes the file if it is to short otherwise. (IMPL!)
  Chunk getChunk(unsigned int index, bool write = false, bool resize = true);

  // chunks with no Part's automatically fails the hash check.
  bool doneChunk(Chunk& c);

  const HashVector& hashes() const { return m_hashes; }
  const BitField& bitfield() const { return m_bitfield; }

private:
  void createDirs();
  static void makeDir(const std::string& dir);

  FileVector   m_files;
  HashVector   m_hashes;
  BitField     m_bitfield;

  uint64_t     m_doneSize;
  uint64_t     m_totalSize;
  unsigned int m_chunkSize;
  unsigned int m_chunkLast;
  unsigned int m_completed;

  std::string m_rootDir;
};

} // namespace torrent

#endif // LIBTORRENT_FILES_H

