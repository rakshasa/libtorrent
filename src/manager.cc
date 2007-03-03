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

#include "config.h"

#include "torrent/exceptions.h"

#include "download/choke_manager.h"
#include "download/download_manager.h"
#include "download/download_wrapper.h"
#include "download/download_main.h"
#include "data/hash_torrent.h"
#include "protocol/handshake_manager.h"
#include "data/hash_queue.h"
#include "net/throttle_manager.h"
#include "net/listen.h"

#include "torrent/chunk_manager.h"
#include "torrent/connection_manager.h"
#include "torrent/data/file_manager.h"
#include "torrent/peer/client_list.h"

#include "manager.h"
#include "resource_manager.h"

namespace torrent {

Manager* manager = NULL;

Manager::Manager() :
  m_downloadManager(new DownloadManager),
  m_fileManager(new FileManager),
  m_handshakeManager(new HandshakeManager),
  m_hashQueue(new HashQueue),
  m_resourceManager(new ResourceManager),

  m_chunkManager(new ChunkManager),
  m_clientList(new ClientList),
  m_connectionManager(new ConnectionManager),

  m_poll(NULL),

  m_uploadThrottle(new ThrottleManager),
  m_downloadThrottle(new ThrottleManager),

  m_ticks(0) {

  m_taskTick.set_slot(rak::mem_fn(this, &Manager::receive_tick));

  priority_queue_insert(&taskScheduler, &m_taskTick, cachedTime.round_seconds());

  m_handshakeManager->slot_download_id(rak::make_mem_fun(m_downloadManager, &DownloadManager::find_main));
  m_handshakeManager->slot_download_id_obfuscated(rak::make_mem_fun(m_downloadManager, &DownloadManager::find_main_obfuscated));
  m_connectionManager->listen()->slot_incoming(rak::make_mem_fun(m_handshakeManager, &HandshakeManager::add_incoming));
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
  delete m_connectionManager;
  delete m_chunkManager;

  delete m_clientList;

  delete m_uploadThrottle;
  delete m_downloadThrottle;
}

void
Manager::initialize_download(DownloadWrapper* d) {
  d->main()->slot_count_handshakes(rak::make_mem_fun(m_handshakeManager, &HandshakeManager::size_info));
  d->main()->slot_start_handshake(rak::make_mem_fun(m_handshakeManager, &HandshakeManager::add_outgoing));
  d->main()->slot_stop_handshakes(rak::make_mem_fun(m_handshakeManager, &HandshakeManager::erase_download));

  m_downloadManager->insert(d);
  m_resourceManager->insert(d->main(), 1);
  m_chunkManager->insert(d->main()->chunk_list());

  d->main()->set_upload_throttle(m_uploadThrottle->throttle_list());
  d->main()->set_download_throttle(m_downloadThrottle->throttle_list());

  d->main()->upload_choke_manager()->slot_choke(rak::make_mem_fun(manager->resource_manager(), &ResourceManager::receive_upload_choke));
  d->main()->upload_choke_manager()->slot_unchoke(rak::make_mem_fun(manager->resource_manager(), &ResourceManager::receive_upload_unchoke));
  d->main()->upload_choke_manager()->slot_can_unchoke(rak::make_mem_fun(manager->resource_manager(), &ResourceManager::retrieve_upload_can_unchoke));

  d->main()->download_choke_manager()->slot_choke(rak::make_mem_fun(manager->resource_manager(), &ResourceManager::receive_download_choke));
  d->main()->download_choke_manager()->slot_unchoke(rak::make_mem_fun(manager->resource_manager(), &ResourceManager::receive_download_unchoke));
  d->main()->download_choke_manager()->slot_can_unchoke(rak::make_mem_fun(manager->resource_manager(), &ResourceManager::retrieve_download_can_unchoke));
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

  m_resourceManager->receive_tick();
  m_chunkManager->periodic_sync();

  std::for_each(m_downloadManager->begin(), m_downloadManager->end(), std::bind2nd(std::mem_fun(&DownloadWrapper::receive_tick), m_ticks));

  // If you change the interval, make sure the keepalives gets
  // triggered every 120 seconds.
  priority_queue_insert(&taskScheduler, &m_taskTick, (cachedTime + rak::timer::from_seconds(30)).round_seconds());
}

}
