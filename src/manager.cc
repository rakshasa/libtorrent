#include "config.h"

#include "torrent/exceptions.h"

#include "download/download_wrapper.h"
#include "download/download_main.h"
#include "data/hash_torrent.h"
#include "data/chunk_list.h"
#include "protocol/handshake_manager.h"
#include "data/hash_queue.h"
#include "net/listen.h"
#include "utils/instrumentation.h"

#include "torrent/chunk_manager.h"
#include "torrent/connection_manager.h"
#include "torrent/dht_manager.h"
#include "torrent/data/file_manager.h"
#include "torrent/download/choke_group.h"
#include "torrent/download/choke_queue.h"
#include "torrent/download/download_manager.h"
#include "torrent/download/resource_manager.h"
#include "torrent/peer/client_list.h"
#include "torrent/throttle.h"

#include "manager.h"

namespace torrent {

Manager* manager = NULL;

Manager::Manager() :
  m_downloadManager(new DownloadManager),
  m_fileManager(new FileManager),
  m_handshakeManager(new HandshakeManager),
  m_resourceManager(new ResourceManager),

  m_chunkManager(new ChunkManager),
  m_clientList(new ClientList),
  m_connectionManager(new ConnectionManager),
  m_dhtManager(new DhtManager),

  m_uploadThrottle(Throttle::create_throttle()),
  m_downloadThrottle(Throttle::create_throttle()),

  m_ticks(0) {

  m_hashQueue = new HashQueue(&m_main_thread_disk);
  m_hashQueue->slot_has_work() =
    std::bind(&thread_base::send_event_signal,
              &m_main_thread_main,
              m_main_thread_main.signal_bitfield()->add_signal(std::bind(&HashQueue::work, m_hashQueue)),
              std::placeholders::_1);

  m_taskTick.slot() = std::bind(&Manager::receive_tick, this);

  priority_queue_insert(&taskScheduler, &m_taskTick, cachedTime.round_seconds());

  m_handshakeManager->slot_download_id() =
    std::bind(&DownloadManager::find_main, m_downloadManager, std::placeholders::_1);
  m_handshakeManager->slot_download_obfuscated() =
    std::bind(&DownloadManager::find_main_obfuscated, m_downloadManager, std::placeholders::_1);
  m_connectionManager->listen()->slot_accepted() =
    std::bind(&HandshakeManager::add_incoming, m_handshakeManager, std::placeholders::_1, std::placeholders::_2);

  m_resourceManager->push_group("default");
  m_resourceManager->group_back()->up_queue()->set_heuristics(choke_queue::HEURISTICS_UPLOAD_LEECH);
  m_resourceManager->group_back()->down_queue()->set_heuristics(choke_queue::HEURISTICS_DOWNLOAD_LEECH);
}

Manager::~Manager() {
  priority_queue_erase(&taskScheduler, &m_taskTick);

  m_handshakeManager->clear();
  m_downloadManager->clear();

  delete m_downloadManager;
  delete m_fileManager;
  delete m_handshakeManager;
  delete m_hashQueue;

  delete m_resourceManager;
  delete m_dhtManager;
  delete m_connectionManager;
  delete m_chunkManager;

  delete m_clientList;

  Throttle::destroy_throttle(m_uploadThrottle);
  Throttle::destroy_throttle(m_downloadThrottle);

  instrumentation_tick();
}

void
Manager::initialize_download(DownloadWrapper* d) {
  d->main()->slot_count_handshakes(rak::make_mem_fun(m_handshakeManager, &HandshakeManager::size_info));
  d->main()->slot_start_handshake(rak::make_mem_fun(m_handshakeManager, &HandshakeManager::add_outgoing));
  d->main()->slot_stop_handshakes(rak::make_mem_fun(m_handshakeManager, &HandshakeManager::erase_download));

  // TODO: The resource manager doesn't need to know about this
  // download until we start/stop the torrent.
  m_downloadManager->insert(d);
  m_resourceManager->insert(d->main(), 1);
  m_chunkManager->insert(d->main()->chunk_list());

  d->main()->chunk_list()->set_chunk_size(d->main()->file_list()->chunk_size());

  d->main()->set_upload_throttle(m_uploadThrottle->throttle_list());
  d->main()->set_download_throttle(m_downloadThrottle->throttle_list());
}

void
Manager::cleanup_download(DownloadWrapper* d) {
  d->main()->stop();
  d->close();

  m_resourceManager->erase(d->main());
  m_chunkManager->erase(d->main()->chunk_list());

  m_downloadManager->erase(d);
}

void
Manager::receive_tick() {
  m_ticks++;

  if (m_ticks % 2 == 0)
    instrumentation_tick();

  m_resourceManager->receive_tick();
  m_chunkManager->periodic_sync();

  // To ensure the downloads get equal chance over time at using
  // various limited resources, like sockets for handshakes, cycle the
  // group in reverse order.
  if (!m_downloadManager->empty()) {
    DownloadManager::iterator split = m_downloadManager->end() - m_ticks % m_downloadManager->size() - 1;

    std::for_each(split, m_downloadManager->end(),   std::bind2nd(std::mem_fun(&DownloadWrapper::receive_tick), m_ticks));
    std::for_each(m_downloadManager->begin(), split, std::bind2nd(std::mem_fun(&DownloadWrapper::receive_tick), m_ticks));
  }

  // If you change the interval, make sure the keepalives gets
  // triggered every 120 seconds.
  priority_queue_insert(&taskScheduler, &m_taskTick, (cachedTime + rak::timer::from_seconds(30)).round_seconds());
}

}
