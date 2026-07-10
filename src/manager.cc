#include "config.h"

#include "manager.h"

#include "data/chunk_list.h"
#include "data/chunk_manager.h"
#include "data/hash_torrent.h"
#include "download/download_main.h"
#include "download/download_wrapper.h"
#include "net/listen.h"
#include "protocol/handshake_manager.h"
#include "torrent/throttle.h"
#include "torrent/data/file_manager.h"
#include "torrent/download/choke_group.h"
#include "torrent/download/choke_queue.h"
#include "torrent/download/download_manager.h"
#include "torrent/download/resource_manager.h"
#include "torrent/peer/client_list.h"
#include "torrent/runtime/network_manager.h"
#include "torrent/runtime/socket_manager.h"
#include "utils/instrumentation.h"
#include "utils/thread_internal.h"

namespace torrent {

Manager* manager = nullptr;

Manager::Manager()
  : m_chunk_manager(new ChunkManager),
    m_download_manager(new DownloadManager),
    m_file_manager(new FileManager),
    m_handshake_manager(new HandshakeManager),
    m_resource_manager(new ResourceManager),

    m_client_list(new ClientList),

    m_uploadThrottle(Throttle::create_throttle()),
    m_downloadThrottle(Throttle::create_throttle()) {

  m_task_tick.slot() = [this] { receive_tick(); };
  torrent::this_thread::scheduler()->wait_for_ceil_seconds(&m_task_tick, 1s);

  m_handshake_manager->slot_download_id()         = [this](auto hash) { return m_download_manager->find_main(hash); };
  m_handshake_manager->slot_download_obfuscated() = [this](auto hash) { return m_download_manager->find_main_obfuscated(hash); };

  runtime::network_manager()->listen_inet_unsafe()->slot_accepted()  = [this](auto& handshake, auto fd, auto sa) { return m_handshake_manager->add_incoming(handshake, fd, sa); };
  runtime::network_manager()->listen_inet6_unsafe()->slot_accepted() = [this](auto& handshake, auto fd, auto sa) { return m_handshake_manager->add_incoming(handshake, fd, sa); };

  m_resource_manager->push_group("default");
  m_resource_manager->group_back()->up_queue()->set_heuristics(HEURISTICS_UPLOAD_LEECH);
  m_resource_manager->group_back()->down_queue()->set_heuristics(HEURISTICS_DOWNLOAD_LEECH);
}

Manager::~Manager() = default;

void
Manager::cleanup() {
  torrent::this_thread::scheduler()->erase(&m_task_tick);

  m_handshake_manager->clear();
  m_download_manager->clear();

  Throttle::destroy_throttle(m_uploadThrottle);
  Throttle::destroy_throttle(m_downloadThrottle);

  instrumentation_tick();
}

void
Manager::initialize_download(DownloadWrapper* d) {
  d->main()->slot_count_handshakes([this](DownloadMain* download) {
      return m_handshake_manager->size_info(download);
    });
  d->main()->slot_start_handshake([this](const sockaddr* sa, DownloadMain* download) {
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
  torrent::this_thread::scheduler()->wait_for_ceil_seconds(&m_task_tick, 30s);
}

} // namespace torrent
