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

  auto*               network_config()     { return m_network_config.get(); }

  auto*               chunk_manager()      { return m_chunk_manager.get(); }
  auto*               connection_manager() { return m_connection_manager.get(); }
  auto*               download_manager()   { return m_download_manager.get(); }
  auto*               file_manager()       { return m_file_manager.get(); }
  auto*               handshake_manager()  { return m_handshake_manager.get(); }
  auto*               network_manager()    { return m_network_manager.get(); }
  auto*               resource_manager()   { return m_resource_manager.get(); }

  auto*               client_list()        { return m_client_list.get(); }

  EncodingList*       encoding_list()      { return &m_encodingList; }

  Throttle*           upload_throttle()    { return m_uploadThrottle; }
  Throttle*           download_throttle()  { return m_downloadThrottle; }

  void                initialize_download(DownloadWrapper* d);
  void                cleanup_download(DownloadWrapper* d);

  void                receive_tick();

private:
  std::unique_ptr<net::NetworkConfig>  m_network_config;

  std::unique_ptr<ChunkManager>        m_chunk_manager;
  std::unique_ptr<ConnectionManager>   m_connection_manager;
  std::unique_ptr<DownloadManager>     m_download_manager;
  std::unique_ptr<FileManager>         m_file_manager;
  std::unique_ptr<HandshakeManager>    m_handshake_manager;
  std::unique_ptr<net::NetworkManager> m_network_manager;
  std::unique_ptr<ResourceManager>     m_resource_manager;

  std::unique_ptr<ClientList>             m_client_list;

  EncodingList        m_encodingList;

  Throttle*           m_uploadThrottle;
  Throttle*           m_downloadThrottle;

  unsigned int          m_ticks{0};
  utils::SchedulerEntry m_task_tick;
};

extern Manager* manager;

} // namespace torrent

#endif
