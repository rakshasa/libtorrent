#ifndef LIBTORRENT_CONTENT_H
#define LIBTORRENT_CONTENT_H

#include <inttypes.h>
#include <string>
#include <sigc++/signal.h>

#include "utils/bitfield.h"
#include "content_file.h"
#include "data/storage.h"

namespace torrent {

// Since g++ uses reference counted strings, it's cheaper to just hand
// over bencode's string.

class Content {
public:
  typedef std::vector<ContentFile> FileList;
  typedef sigc::signal0<void>      SignalDownloadDone;
  // Hash done signal, hash failed signal++

  Content() : m_size(0), m_completed(0), m_rootDir(".") {}

  // Do not modify chunk size after files have been added.
  void                   add_file(const Path& path, uint64_t size);

  void                   set_complete_hash(const std::string& hash);
  void                   set_root_dir(const std::string& path);

  std::string            get_hash(unsigned int index);
  const char*            get_hash_c(unsigned int index)  { return m_hash.c_str() + 20 * index; }
  const std::string&     get_complete_hash()             { return m_hash; }
  const std::string&     get_root_dir()                  { return m_rootDir; }

  uint64_t               get_size()                      { return m_size; }
  uint32_t               get_chunks_completed()          { return m_completed; }
  uint64_t               get_bytes_completed();

  uint32_t               get_chunksize(uint32_t index);

  const BitField&        get_bitfield()                  { return m_bitfield; }
  FileList&              get_files()                     { return m_files; }
  Storage&               get_storage()                   { return m_storage; }

  bool                   is_open()                       { return m_storage.get_size(); }
  bool                   is_correct_size();
  bool                   is_done()                       { return m_completed == m_storage.get_chunkcount(); }

  void                   open(bool wr = false);
  void                   close();

  void                   resize();

  void                   mark_done(uint32_t index);

  SignalDownloadDone&    signal_download_done()          { return m_downloadDone; }

private:
  
  void                   open_file(File* f, Path& p, Path& lastPath);

  uint64_t               m_size;
  uint32_t               m_completed;

  FileList               m_files;
  Storage                m_storage;

  BitField               m_bitfield;

  std::string            m_rootDir;
  std::string            m_hash;

  SignalDownloadDone     m_downloadDone;
};

}

#endif
