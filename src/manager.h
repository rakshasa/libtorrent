// libTorrent - BitTorrent library
// Copyright (C) 2005-2006, Jari Sundell
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

#include "net/socket_fd.h"

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
class ConnectionManager;
class ThrottleManager;

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

  ConnectionManager*  connection_manager()                      { return m_connectionManager; }
  
  Poll*               poll()                                    { return m_poll; }
  void                set_poll(Poll* p)                         { m_poll = p; }

  EncodingList*       encoding_list()                           { return &m_encodingList; }

  ThrottleManager*    upload_throttle()                         { return m_uploadThrottle; }
  ThrottleManager*    download_throttle()                       { return m_downloadThrottle; }

  void                initialize_download(DownloadWrapper* d);
  void                cleanup_download(DownloadWrapper* d);

  void                receive_tick();
  void                receive_connection(SocketFd fd, DownloadMain* download, const PeerInfo& peer);

private:
  DownloadManager*    m_downloadManager;
  FileManager*        m_fileManager;
  HandshakeManager*   m_handshakeManager;
  HashQueue*          m_hashQueue;
  ResourceManager*    m_resourceManager;

  ConnectionManager*  m_connectionManager;
  Poll*               m_poll;

  EncodingList        m_encodingList;

  ThrottleManager*    m_uploadThrottle;
  ThrottleManager*    m_downloadThrottle;

  unsigned int        m_ticks;
  rak::priority_item  m_taskTick;
};

extern Manager* manager;

}

#endif
