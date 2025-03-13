#include "config.h"

#include "torrent/exceptions.h"

#include "dht/dht_router.h"
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
#include "torrent/data/file_manager.h"
#include "torrent/download/choke_group.h"
#include "torrent/download/choke_queue.h"
#include "torrent/download/download_manager.h"
#include "torrent/download/resource_manager.h"
#include "torrent/peer/client_list.h"
#include "torrent/throttle.h"
#include "torrent/tracker/dht_controller.h"
#include "torrent/tracker/manager.h"

#include "manager.h"

namespace torrent {

Manager* manager = NULL;

Manager::Manager() :
  m_chunk_manager(new ChunkManager),
  m_connection_manager(new ConnectionManager),
  m_download_manager(new DownloadManager),
  m_file_manager(new FileManager),
  m_handshake_manager(new HandshakeManager),
  m_resource_manager(new ResourceManager),

  m_client_list(new ClientList),
  m_dht_controller(new tracker::DhtController),

  m_uploadThrottle(Throttle::create_throttle()),
  m_downloadThrottle(Throttle::create_throttle()) {

  m_hash_queue = std::make_unique<HashQueue>(&m_thread_disk);

  auto hash_work_signal = m_thread_main.signal_bitfield()->add_signal([hash_queue = m_hash_queue.get()]() {
      return hash_queue->work();
    });
  m_hash_queue->slot_has_work() = [hash_work_signal, thread = &m_thread_main](bool is_done) {
      thread->send_event_signal(hash_work_signal, is_done);
    };

  m_taskTick.slot() = std::bind(&Manager::receive_tick, this);

  priority_queue_insert(&taskScheduler, &m_taskTick, cachedTime.round_seconds());

  m_handshake_manager->slot_download_id() = [this](auto hash) { return m_download_manager->find_main(hash); };
  m_handshake_manager->slot_download_obfuscated() = [this](auto hash) { return m_download_manager->find_main_obfuscated(hash); };
  m_connection_manager->listen()->slot_accepted() = [this](auto fd, auto sa) { return m_handshake_manager->add_incoming(fd, sa); };

  m_resource_manager->push_group("default");
  m_resource_manager->group_back()->up_queue()->set_heuristics(choke_queue::HEURISTICS_UPLOAD_LEECH);
  m_resource_manager->group_back()->down_queue()->set_heuristics(choke_queue::HEURISTICS_DOWNLOAD_LEECH);
}

Manager::~Manager() {
  priority_queue_erase(&taskScheduler, &m_taskTick);

  m_handshake_manager->clear();
  m_download_manager->clear();
  m_dht_controller.reset();

  Throttle::destroy_throttle(m_uploadThrottle);
  Throttle::destroy_throttle(m_downloadThrottle);

  instrumentation_tick();
}

void
Manager::initialize_download(DownloadWrapper* d) {
  d->main()->slot_count_handshakes([this](DownloadMain* download) {
    return m_handshake_manager->size_info(download);
  });
  d->main()->slot_start_handshake([this](const rak::socket_address& sa, DownloadMain* download) {
    return m_handshake_manager->add_outgoing(sa, download);
  });
  d->main()->slot_stop_handshakes([this](DownloadMain* download) {
    return m_handshake_manager->erase_download(download);
  });

  // TODO: The resource manager doesn't need to know about this
  // download until we start/stop the torrent.
  m_download_manager->insert(d);
  m_resource_manager->insert(d->main(), 1);
  m_chunk_manager->insert(d->main()->chunk_list());

  d->main()->chunk_list()->set_chunk_size(d->main()->file_list()->chunk_size());

  d->main()->set_upload_throttle(m_uploadThrottle->throttle_list());
  d->main()->set_download_throttle(m_downloadThrottle->throttle_list());
}

void
Manager::cleanup_download(DownloadWrapper* d) {
  d->main()->stop();
  d->close();

  m_resource_manager->erase(d->main());
  m_chunk_manager->erase(d->main()->chunk_list());

  m_download_manager->erase(d);
}

void
Manager::receive_tick() {
  m_ticks++;

  if (m_ticks % 2 == 0)
    instrumentation_tick();

  m_resource_manager->receive_tick();
  m_chunk_manager->periodic_sync();

  // To ensure the downloads get equal chance over time at using
  // various limited resources, like sockets for handshakes, cycle the
  // group in reverse order.
  if (!m_download_manager->empty()) {
    auto split = m_download_manager->end() - m_ticks % m_download_manager->size() - 1;

    std::for_each(split, m_download_manager->end(), [this](auto wrapper) { return wrapper->receive_tick(m_ticks); });
    std::for_each(m_download_manager->begin(), split, [this](auto wrapper) { return wrapper->receive_tick(m_ticks); });
  }

  // If you change the interval, make sure the keepalives gets
  // triggered every 120 seconds.
  priority_queue_insert(&taskScheduler, &m_taskTick, (cachedTime + rak::timer::from_seconds(30)).round_seconds());
}

}
