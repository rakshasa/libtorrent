#ifndef LIBTORRENT_CONTENT_H
#define LIBTORRENT_CONTENT_H

#include <string>
#include <sigc++/signal.h>

#include "bitfield.h"
#include "bitfield_counter.h"
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

  Content() : m_size(0), m_rootDir(".") {}

  void                   add_file(const ContentFile::FileName& filename, uint64_t size);

  void                   set_complete_hash(const std::string& hash);
  void                   set_root_dir(const std::string& dir);

  std::string            get_hash(unsigned int index);
  const std::string&     get_complete_hash()           { return m_hash; }
  const std::string&     get_root_dir()                { return m_rootDir; }
  uint64_t               get_size()                    { return m_size; }

  const Bitfield&        get_bitfield()                { return m_bitfield; }
  const BitfieldCounter& get_seen()                    { return m_seen; }

  FileList&              get_files()                   { return m_files; }
  Storage&               get_storage()                 { return m_storage; }

  bool                   is_open()                     { return m_storage.get_size(); }
  bool                   is_correct_size();

  void                   open(bool wr = false);
  void                   close();

  void                   resize();

  SignalDownloadDone&    signal_download_done()        { return m_downloadDone; }

private:
  
  uint64_t               m_size;

  FileList               m_files;
  Storage                m_storage;

  BitField               m_bitfield;
  BitFieldCounter        m_seen;

  std::string            m_rootDir;
  std::string            m_hash;

  SignalDownloadDone     m_downloadDone;
};

}

#endif
