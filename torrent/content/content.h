#ifndef LIBTORRENT_CONTENT_H
#define LIBTORRENT_CONTENT_H

#include <sigc++/signal.h>

#include "bitfield.h"
#include "bitfield_counter.h"

namespace torrent {

// Since g++ uses reference counted strings, it's cheaper to just hand
// over bencode's string.

class Content {
public:
  typedef std::vector<ContentFile> FileList;
  typedef std::list<std::string>   FileName;
  typedef sigc::signal0<void>      SignalDownloadDone;
  // Hash done signal, hash failed signal++

  void                   add_file(const FileName& filename, uint64_t length);

  void                   set_complete_hash(const std::string& hash);

  const char*            get_hash(unsigned int index);
  const std::string&     get_complete_hash();
  uint64_t               get_length();

  const Bitfield&        get_bitfield();
  const BitfieldCounter& get_bitfield_counter();

  FileList&              get_files();
  Storage&               get_storage();

  bool                   is_open();
  bool                   is_write();
  bool                   is_correct_size();

  void                   open(bool wr = false);
  void                   close();

  void                   resize();

private:
  
