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

  const auto&         chunk_manager() const      { return m_chunk_manager; }
  const auto&         connection_manager() const { return m_connection_manager; }
  const auto&         download_manager() const   { return m_download_manager; }
  const auto&         file_manager() const       { return m_file_manager; }
  const auto&         handshake_manager() const  { return m_handshake_manager; }
  const auto&         resource_manager() const   { return m_resource_manager; }

  const auto&         client_list() const        { return m_client_list; }
  const auto&         dht_controller() const     { return m_dht_controller; }

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

} // namespace torrent

#endif
