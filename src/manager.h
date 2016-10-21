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

#ifndef LIBTORRENT_MANAGER_H
#define LIBTORRENT_MANAGER_H

#include <list>
#include <string>
#include <rak/priority_queue_default.h>

#include "thread_disk.h"
#include "thread_main.h"
#include "net/socket_fd.h"
#include "torrent/utils/udnsevent.h"

namespace torrent {

class Poll;

class HashQueue;
class HandshakeManager;
class DownloadManager;
class DownloadWrapper;
class DownloadMain;
class FileManager;
class ResourceManager;
class PeerInfo;
class ChunkManager;
class ConnectionManager;
class Throttle;
class DhtManager;

typedef std::list<std::string> EncodingList;

class Manager {
public:
  Manager();
  ~Manager();

  DownloadManager*    download_manager()                        { return m_downloadManager; }
  FileManager*        file_manager()                            { return m_fileManager; }
  HandshakeManager*   handshake_manager()                       { return m_handshakeManager; }
  HashQueue*          hash_queue()                              { return m_hashQueue; }
  ResourceManager*    resource_manager()                        { return m_resourceManager; }

  ChunkManager*       chunk_manager()                           { return m_chunkManager; }
  ClientList*         client_list()                             { return m_clientList; }
  ConnectionManager*  connection_manager()                      { return m_connectionManager; }
  DhtManager*         dht_manager()                             { return m_dhtManager; }
  
  Poll*               poll()                                    { return m_main_thread_main.poll(); }

  thread_main*        main_thread_main()                        { return &m_main_thread_main; }
  thread_disk*        main_thread_disk()                        { return &m_main_thread_disk; }

  EncodingList*       encoding_list()                           { return &m_encodingList; }

  Throttle*           upload_throttle()                         { return m_uploadThrottle; }
  Throttle*           download_throttle()                       { return m_downloadThrottle; }

  // TODO put this somewhere better?
  UdnsEvent           udnsevent;

  void                initialize_download(DownloadWrapper* d);
  void                cleanup_download(DownloadWrapper* d);

  void                receive_tick();

private:
  DownloadManager*    m_downloadManager;
  FileManager*        m_fileManager;
  HandshakeManager*   m_handshakeManager;
  HashQueue*          m_hashQueue;
  ResourceManager*    m_resourceManager;

  ChunkManager*       m_chunkManager;
  ClientList*         m_clientList;
  ConnectionManager*  m_connectionManager;
  DhtManager*         m_dhtManager;

  thread_main         m_main_thread_main;
  thread_disk         m_main_thread_disk;

  EncodingList        m_encodingList;

  Throttle*           m_uploadThrottle;
  Throttle*           m_downloadThrottle;

  unsigned int        m_ticks;
  rak::priority_item  m_taskTick;

};

extern Manager* manager;

}

#endif
