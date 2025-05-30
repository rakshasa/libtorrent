#ifndef LIBTORRENT_MANAGER_H
#define LIBTORRENT_MANAGER_H

#include <list>
#include <memory>
#include <string>

#include "torrent/common.h"
#include "torrent/utils/scheduler.h"

namespace torrent {

class DownloadManager;
class FileManager;
class ResourceManager;

using EncodingList = std::list<std::string>;

class Manager {
public:
  Manager();
  ~Manager();

  void                cleanup();

  ChunkManager*       chunk_manager()      { return m_chunk_manager.get(); }
  ConnectionManager*  connection_manager() { return m_connection_manager.get(); }
  DownloadManager*    download_manager()   { return m_download_manager.get(); }
  FileManager*        file_manager()       { return m_file_manager.get(); }
  HandshakeManager*   handshake_manager()  { return m_handshake_manager.get(); }
  ResourceManager*    resource_manager()   { return m_resource_manager.get(); }

  ClientList*             client_list()    { return m_client_list.get(); }
  tracker::DhtController* dht_controller() { return m_dht_controller.get(); }

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

  EncodingList        m_encodingList;

  Throttle*           m_uploadThrottle;
  Throttle*           m_downloadThrottle;

  unsigned int          m_ticks{0};
  utils::SchedulerEntry m_task_tick;
};

extern Manager* manager;

}

#endif
