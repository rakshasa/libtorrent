// libTorrent - BitTorrent library
// Copyright (C) 2005-2011, Jari Sundell
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
// In addition, as a special exception, the copyright holders give
// permission to link the code of portions of this program with the
// OpenSSL library under certain conditions as described in each
// individual source file, and distribute linked combinations
// including the two.
//
// You must obey the GNU General Public License in all respects for
// all of the code used other than OpenSSL.  If you modify file(s)
// with this exception, you may extend this exception to your version
// of the file(s), but you are not obligated to do so.  If you do not
// wish to do so, delete this exception statement from your version.
// If you delete this exception statement from all source files in the
// program, then also delete it here.
//
// Contact:  Jari Sundell <jaris@ifi.uio.no>
//
//           Skomakerveien 33
//           3185 Skoppum, NORWAY

#ifndef LIBTORRENT_DOWNLOAD_WRAPPER_H
#define LIBTORRENT_DOWNLOAD_WRAPPER_H

#include "data/chunk_handle.h"
#include "download_main.h"

namespace torrent {

// Remember to clean up the pointers, DownloadWrapper won't do it.

class AddressList;
class FileManager;
class HashQueue;
class HashTorrent;
class HandshakeManager;
class DownloadInfo;
class Object;
class Peer;

class DownloadWrapper {
public:
  DownloadWrapper();
  ~DownloadWrapper();

  DownloadInfo*       info()                                  { return m_main->info(); }
  download_data*      data()                                  { return m_main->file_list()->mutable_data(); }
  FileList*           file_list()                             { return m_main->file_list(); }
  ChunkList*          chunk_list()                            { return m_main->chunk_list(); }

  // Initialize hash checker and various download stuff.
  void                initialize(const std::string& hash, const std::string& id);

  void                close();

  bool                is_stopped() const;

  DownloadMain*       main()                                  { return m_main; }
  const DownloadMain* main() const                            { return m_main; }
  HashTorrent*        hash_checker()                          { return m_hashChecker; }

  Object*             bencode()                               { return m_bencode; }
  void                set_bencode(Object* o)                  { m_bencode = o; }

  HashQueue*          hash_queue()                            { return m_hashQueue; }
  void                set_hash_queue(HashQueue* q)            { m_hashQueue = q; }

  const std::string&  complete_hash()                            { return m_hash; }
  const char*         chunk_hash(unsigned int index)             { return m_hash.c_str() + 20 * index; }
  void                set_complete_hash(const std::string& hash) { m_hash = hash; }

  int                 connection_type() const                 { return m_connectionType; }
  void                set_connection_type(int t)              { m_connectionType = t; }

  //
  // Internal:
  //

  void                receive_initial_hash();
  void                receive_hash_done(ChunkHandle handle, const char* hash);

  void                check_chunk_hash(ChunkHandle handle);

  void                receive_storage_error(const std::string& str);
  uint32_t            receive_tracker_success(AddressList* l);
  void                receive_tracker_failed(const std::string& msg);

  void                receive_tick(uint32_t ticks);

  void                receive_update_priorities();

private:
  DownloadWrapper(const DownloadWrapper&);
  void operator = (const DownloadWrapper&);

  void                finished_download();

  DownloadMain*       m_main;
  Object*             m_bencode;
  HashTorrent*        m_hashChecker;
  HashQueue*          m_hashQueue;

  std::string         m_hash;

  int                 m_connectionType;
};

}

#endif
