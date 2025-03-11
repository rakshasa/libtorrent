#ifndef LIBTORRENT_MANAGER_H
#define LIBTORRENT_MANAGER_H

#include <list>
#include <memory>
#include <string>

#include "thread_main.h"
#include "data/thread_disk.h"
#include "rak/priority_queue_default.h"
#include "torrent/common.h"

namespace torrent {

class DownloadManager;
class FileManager;
class HashQueue;
class ResourceManager;

typedef std::list<std::string> EncodingList;

class Manager {
public:
  Manager();
  ~Manager();

  ChunkManager*       chunk_manager()      { return m_chunk_manager.get(); }
  ConnectionManager*  connection_manager() { return m_connection_manager.get(); }
  DownloadManager*    download_manager()   { return m_download_manager.get(); }
  FileManager*        file_manager()       { return m_file_manager.get(); }
  HandshakeManager*   handshake_manager()  { return m_handshake_manager.get(); }
  ResourceManager*    resource_manager()   { return m_resource_manager.get(); }

  ClientList*             client_list()    { return m_client_list.get(); }
  tracker::DhtController* dht_controller() { return m_dht_controller.get(); }
  HashQueue*              hash_queue()     { return m_hash_queue.get(); }

  Poll*               poll()               { return m_thread_main.poll(); }

  ThreadMain*         thread_main()        { return &m_thread_main; }
  ThreadDisk*         thread_disk()        { return &m_thread_disk; }

  EncodingList*       encoding_list()      { return &m_encodingList; }

  Throttle*           upload_throttle()    { return m_uploadThrottle; }
  Throttle*           download_throttle()  { return m_downloadThrottle; }

  void                initialize_download(DownloadWrapper* d);
  void                cleanup_download(DownloadWrapper* d);

  void                receive_tick();

private:
  std::unique_ptr<ChunkManager>      m_chunk_manager;
  std::unique_ptr<ConnectionManager> m_connection_manager;
  std::unique_ptr<DownloadManager>   m_download_manager;
  std::unique_ptr<FileManager>       m_file_manager;
  std::unique_ptr<HandshakeManager>  m_handshake_manager;
  std::unique_ptr<ResourceManager>   m_resource_manager;

  std::unique_ptr<ClientList>             m_client_list;
  std::unique_ptr<tracker::DhtController> m_dht_controller;
  std::unique_ptr<HashQueue>              m_hash_queue;

  ThreadMain          m_thread_main;
  ThreadDisk          m_thread_disk;

  EncodingList        m_encodingList;

  Throttle*           m_uploadThrottle;
  Throttle*           m_downloadThrottle;

  unsigned int        m_ticks{0};
  rak::priority_item  m_taskTick;
};

extern Manager* manager;

}

#endif
