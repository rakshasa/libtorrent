// libTorrent - BitTorrent library
// Copyright (C) 2005-2011, Jari Sundell
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

namespace tr1 { using namespace std::tr1; }

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
    tr1::bind(&thread_base::send_event_signal,
              &m_main_thread_main,
              m_main_thread_main.signal_bitfield()->add_signal(tr1::bind(&HashQueue::work, m_hashQueue)),
              tr1::placeholders::_1);

  m_taskTick.slot() = std::tr1::bind(&Manager::receive_tick, this);

  priority_queue_insert(&taskScheduler, &m_taskTick, cachedTime.round_seconds());

  m_handshakeManager->slot_download_id() =
    std::tr1::bind(&DownloadManager::find_main, m_downloadManager, std::tr1::placeholders::_1);
  m_handshakeManager->slot_download_obfuscated() =
    std::tr1::bind(&DownloadManager::find_main_obfuscated, m_downloadManager, std::tr1::placeholders::_1);
  m_connectionManager->listen()->slot_accepted() =
    std::tr1::bind(&HandshakeManager::add_incoming, m_handshakeManager, std::tr1::placeholders::_1, std::tr1::placeholders::_2);

  // m_resourceManager->push_group("default");
  // m_resourceManager->group_back()->up_queue()->set_heuristics(choke_queue::HEURISTICS_UPLOAD_LEECH);
  // m_resourceManager->group_back()->down_queue()->set_heuristics(choke_queue::HEURISTICS_DOWNLOAD_LEECH);
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
