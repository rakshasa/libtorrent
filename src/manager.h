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

#include <string>

#include "net/socket_address.h"
#include "utils/task_item.h"

namespace torrent {

class Listen;
class HashQueue;
class HandshakeManager;
class DownloadManager;
class DownloadWrapper;
class FileManager;
class ResourceManager;

class Manager {
public:
  Manager();
  ~Manager();

  DownloadManager*    download_manager()                        { return m_downloadManager; }
  FileManager*        file_manager()                            { return m_fileManager; }
  HandshakeManager*   handshake_manager()                       { return m_handshakeManager; }
  HashQueue*          hash_queue()                              { return m_hashQueue; }
  Listen*             listen()                                  { return m_listen; }
  ResourceManager*    resource_manager()                        { return m_resourceManager; }

  SocketAddress&      get_local_address()                       { return m_localAddress; }
  
  const std::string&  get_bind_address()                        { return m_bindAddress; }
  void                set_bind_address(const std::string& addr) { m_bindAddress = addr; }

  void                initialize_download(DownloadWrapper* d);
  void                cleanup_download(DownloadWrapper* d);

  void                receive_tick();

private:
  SocketAddress       m_localAddress;
  std::string         m_bindAddress;

  DownloadManager*    m_downloadManager;
  FileManager*        m_fileManager;
  HandshakeManager*   m_handshakeManager;
  HashQueue*          m_hashQueue;
  Listen*             m_listen;
  ResourceManager*    m_resourceManager;

  unsigned int        m_ticks;
  TaskItem            m_taskTick;
};

extern Manager* manager;

}

#endif
