// libTorrent - BitTorrent library
// Copyright (C) 2005, Jari Sundell
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
// Contact:  Jari Sundell <jaris@ifi.uio.no>
//
//           Skomakerveien 33
//           3185 Skoppum, NORWAY

#ifndef LIBTORRENT_DOWNLOAD_WRAPPER_H
#define LIBTORRENT_DOWNLOAD_WRAPPER_H

#include <memory>

#include "data/hash_torrent.h"
#include "torrent/bencode.h"
#include "download_main.h"

namespace torrent {

// Remember to clean up the pointers, DownloadWrapper won't do it.

  class FileManager;
class HashQueue;
class HandshakeManager;

class DownloadWrapper {
public:
  DownloadWrapper() {}

  // Initialize hash checker and various download stuff.
  void                initialize(const std::string& hash, const std::string& id);

  // Don't load unless the object is newly initialized.
  void                hash_load();
  void                hash_save();

  void                open();
  void                stop();

  bool                is_stopped()        { return m_main.is_stopped(); }

  const std::string&  get_hash()          { return m_main.get_hash(); }
  DownloadMain&       get_main()          { return m_main; }
  const DownloadMain& get_main() const    { return m_main; }

  Bencode&            get_bencode()       { return m_bencode; }
  HashTorrent&        get_hash_checker()  { return *m_hash.get(); }

  void                set_file_manager(FileManager* f);
  void                set_handshake_manager(HandshakeManager* h);
  void                set_hash_queue(HashQueue* h);

  // Various functions for manipulating bencode's data with the
  // download.
  
private:
  DownloadWrapper(const DownloadWrapper&);
  void operator = (const DownloadWrapper&);

  DownloadMain               m_main;
  Bencode                    m_bencode;
  std::auto_ptr<HashTorrent> m_hash;
};

}

#endif
