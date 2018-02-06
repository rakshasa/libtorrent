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

#include "webseed_controller.h"

#include "config.h"

#include <cstring>

#include "utils/log.h"
#include <curl/curl.h>
#include "torrent/utils/log.h"
#include "protocol/peer_chunks.h"
#include "download/download_main.h"
#include "download/chunk_selector.h"
#include "torrent/data/file_list.h"
#include "torrent/data/block.h"
#include "torrent/data/block_list.h"
#include "torrent/throttle.h"
#include "data/chunk_list.h"
#include "data/hash_chunk.h"
#include "torrent/data/file_list.h"
#include "protocol/request_list.h"
#include "manager.h"
#include "torrent/download/download_manager.h"
#include "download/download_wrapper.h"

namespace torrent {

WebseedController::WebseedController(DownloadMain* download) : m_download(download), m_allChunks(new PeerChunks)
{
}

WebseedController::~WebseedController() {
  m_thread_stop = true;
  if (m_thread.joinable()) m_thread.join();
}

void WebseedController::add_url(const url_type& url) {
  m_url_list.push_back(url);
}

struct MemoryStruct {
  char* memory;
  size_t size;
};

static size_t
WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
  size_t realsize = size * nmemb;
  struct MemoryStruct *mem = (struct MemoryStruct *)userp;
 
  memcpy(&(mem->memory[mem->size]), contents, realsize);

  mem->size += realsize;
 
  return realsize;
}

void WebseedController::download_to_buffer
(
  char* buffer,
  const std::string& url,
  const int offset,
  const int length
)
{
  int end = offset + length - 1;
  std::string range_str = std::to_string(offset) + "-" + std::to_string(end);

  lt_log_print(LOG_DEBUG, "webseed: curl %s %s", url.c_str(), range_str.c_str());

  struct MemoryStruct chunk;
  chunk.memory = buffer;
  chunk.size = 0; 
 
  CURL* curl = curl_easy_init();
  if (!curl) {
    lt_log_print(LOG_ERROR, "webseed: curl init failed");
  } else {
    curl_easy_setopt(curl, CURLOPT_RANGE, range_str.c_str());
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
 
    CURLcode res = curl_easy_perform(curl);
    /* Check for errors */ 
    if(res != CURLE_OK) {
      lt_log_print(LOG_ERROR, "webseed: curl failed to retreive %s: %s", url.c_str(), curl_easy_strerror(res));
    } else {
      lt_log_print(LOG_DEBUG, "webseed: curl received %u bytes", chunk.size);
    }

    /* always cleanup */ 
    curl_easy_cleanup(curl);
  }

}

void WebseedController::start()
{
  if(!url_list()->empty()) {
    m_thread_stop = false;
    m_thread = std::thread(&WebseedController::doWebseed, this);
  }
}

void WebseedController::stop()
{
  m_thread_stop = true;
  if (m_thread.joinable()) m_thread.join();
}

void WebseedController::doWebseed()
{
  lt_log_print(LOG_INFO, "webseed: thread started for %s", m_download->info()->name().c_str());

  //webseeds have all chunks
  m_allChunks->bitfield()->copy(*m_download->file_list()->bitfield());
  m_allChunks->bitfield()->set_all();

  uint32_t chunk_index;
  while (!m_thread_stop && 
         (chunk_index = m_download->chunk_selector()->find(&(*m_allChunks), false)) != Piece::invalid_index)
  {
    download_chunk(chunk_index);
  }
} 

void WebseedController::download_chunk(const uint32_t chunk_index)
{
  uint32_t chunk_size = m_download->file_list()->chunk_index_size(chunk_index);
  Piece piece(chunk_index, 0, chunk_size);

  TransferList::iterator blockListItr = m_download->delegator()->transfer_list()->insert(piece, Delegator::block_size);

  lt_log_print(LOG_DEBUG, "webseed: getting chunk_index %u size %u", chunk_index, chunk_size);

  static const int chunk_flags = ChunkList::get_writable;
  ChunkHandle chunk_handle = m_download->chunk_list()->get(chunk_index, chunk_flags);

  std::for_each(chunk_handle.chunk()->begin(), chunk_handle.chunk()->end(),
                std::bind(&WebseedController::download_chunk_part, this, std::placeholders::_1));

  m_download->delegator()->transfer_list()->erase(blockListItr);

  DownloadWrapper* wrapper = *manager->download_manager()->find(m_download->info());

  char hash[HashString::size_data];
  HashChunk hash_chunk(chunk_handle);
  hash_chunk.perform(hash_chunk.remaining(), false);
  hash_chunk.hash_c(&hash[0]);
  lt_log_print(LOG_DEBUG, "webseed: chunk hash check. should be %s was %s",
    hash_string_to_hex_str(*HashString::cast_from(wrapper->chunk_hash(chunk_handle.index()))).c_str(),
    hash_string_to_hex_str(*HashString::cast_from(hash)).c_str());

  if (std::memcmp(hash, wrapper->chunk_hash(chunk_index), HashString::size_data) == 0)
    m_download->file_list()->mark_completed(chunk_index);

  if (m_download->file_list()->is_done())
    if (!m_download->delay_download_done().is_queued())
      priority_queue_insert(&taskScheduler, &m_download->delay_download_done(), cachedTime);

  m_download->chunk_list()->release(&chunk_handle, chunk_flags);
}

void WebseedController::download_chunk_part(const ChunkPart& chunk_part)
{
  const std::string& absolute_file_path = chunk_part.file()->frozen_path();
  const std::string& root_dir = m_download->file_list()->root_dir();
  std::string relative_file_path = absolute_file_path;
  relative_file_path.replace(relative_file_path.find(root_dir),root_dir.length(),"");

  lt_log_print(LOG_DEBUG, "webseed: chunk_part mapped to file %s offset %u length %u",
                          relative_file_path.c_str(), chunk_part.file_offset(), chunk_part.size());

  std::string http_url = url_list()->front() + m_download->info()->name() + relative_file_path;
  download_to_buffer(chunk_part.chunk().begin(), http_url, chunk_part.file_offset(), chunk_part.size());

  m_download->info()->mutable_down_rate()->insert(chunk_part.size());
  m_download->download_throttle()->node_used(m_allChunks->download_throttle(), chunk_part.size());
} 

}
