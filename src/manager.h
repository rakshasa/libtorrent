#ifndef LIBTORRENT_MANAGER_H
#define LIBTORRENT_MANAGER_H

#include <list>
#include <string>
#include <rak/priority_queue_default.h>

#include "thread_disk.h"
#include "thread_main.h"
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
