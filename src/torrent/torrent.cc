// libTorrent - BitTorrent library
// Copyright (C) 2005, Jari Sundell
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

#include <iostream>
#include <sigc++/bind.h>
#include <rak/functional.h>

#include "exceptions.h"
#include "torrent.h"
#include "bencode.h"

#include "manager.h"
#include "resource_manager.h"

#include "utils/string_manip.h"
#include "utils/throttle.h"
#include "net/listen.h"
#include "net/manager.h"
#include "parse/parse.h"
#include "protocol/handshake_manager.h"
#include "protocol/peer_factory.h"
#include "data/file_manager.h"
#include "data/hash_queue.h"
#include "data/hash_torrent.h"
#include "download/download_manager.h"
#include "download/download_wrapper.h"

namespace torrent {

int64_t Timer::m_cache;

TaskScheduler taskScheduler;
ThrottlePeer  throttleRead;
ThrottlePeer  throttleWrite;

// Find some better way of doing this, or rather... move it outside.
std::string
download_id(const std::string& hash) {
  DownloadManager::iterator itr = manager->download_manager()->find(hash);

  return itr != manager->download_manager()->end() &&
    (*itr)->main()->is_active() &&
    (*itr)->main()->is_checked() ?
    (*itr)->get_local_id() : "";
}

void
receive_connection(SocketFd fd, const std::string& hash, const PeerInfo& peer) {
  DownloadManager::iterator itr = manager->download_manager()->find(hash);
  
  if (itr == manager->download_manager()->end() ||
      !(*itr)->main()->is_active() ||
      !(*itr)->main()->connection_list()->insert((*itr)->main(), peer, fd))
    socketManager.close(fd);
}

uint32_t
calculate_max_open_files(uint32_t openMax) {
  if (openMax >= 8096)
    return 256;
  else if (openMax >= 1024)
    return 128;
  else if (openMax >= 512)
    return 64;
  else if (openMax >= 128)
    return 16;
  else // Assumes we don't try less than 64.
    return 4;
}

uint32_t
calculate_reserved(uint32_t openMax) {
  if (openMax >= 8096)
    return 256;
  else if (openMax >= 1024)
    return 128;
  else if (openMax >= 512)
    return 64;
  else if (openMax >= 128)
    return 32;
  else // Assumes we don't try less than 64.
    return 16;
}    

void
initialize(Poll* poll) {
  if (manager != NULL)
    throw client_error("torrent::initialize(...) called but the library has already been initialized");

  Timer::update();

  manager = new Manager;

  throttleRead.start();
  throttleWrite.start();

  manager->handshake_manager()->slot_connected(sigc::ptr_fun3(&receive_connection));
  manager->handshake_manager()->slot_download_id(sigc::ptr_fun1(download_id));

  pollCustom = poll;

  if (poll->get_open_max() < 64)
    throw client_error("Could not initialize libtorrent, Poll::get_open_max() < 64.");

  uint32_t maxFiles = calculate_max_open_files(poll->get_open_max());

  socketManager.set_max_size(poll->get_open_max() - maxFiles - calculate_reserved(poll->get_open_max()));
  manager->file_manager()->set_max_size(maxFiles);
}

// Clean up and close stuff. Stopping all torrents and waiting for
// them to finish is not required, but recommended.
void
cleanup() {
  if (manager == NULL)
    throw client_error("torrent::cleanup() called but the library is not initialized");

  throttleRead.stop();
  throttleWrite.stop();

  delete manager;
  manager = NULL;

  pollCustom = NULL;
}

bool
listen_open(uint16_t begin, uint16_t end) {
  if (manager == NULL)
    throw client_error("listen_open called but the library has not been initialized");

  SocketAddress sa;

  if (!manager->get_bind_address().empty() && !sa.set_address(manager->get_bind_address()))
    throw local_error("Could not parse the ip address to bind");

  if (!manager->listen()->open(begin, end, sa))
    return false;

  manager->get_local_address().set_port(manager->listen()->get_port());
  manager->handshake_manager()->set_bind_address(sa);

  for (DownloadManager::const_iterator itr = manager->download_manager()->begin(), last = manager->download_manager()->end();
       itr != last; ++itr)
    (*itr)->get_local_address().set_port(manager->listen()->get_port());

  return true;
}

void
listen_close() {
  manager->listen()->close();
}

void
perform() {
  Timer::update();
  taskScheduler.execute(Timer::cache());
}

bool
is_inactive() {
  return manager == NULL ||
    std::find_if(manager->download_manager()->begin(), manager->download_manager()->end(),
		      std::not1(std::mem_fun(&DownloadWrapper::is_stopped)))
    == manager->download_manager()->end();
}

std::string
get_local_address() {
  return !manager->get_local_address().is_address_any() ? manager->get_local_address().get_address() : "";
}

void
set_local_address(const std::string& addr) {
  if (addr == manager->get_local_address().get_address() ||
      !manager->get_local_address().set_address(addr))
    return;

  for (DownloadManager::const_iterator itr = manager->download_manager()->begin(), last = manager->download_manager()->end();
       itr != last; ++itr)
    (*itr)->get_local_address().set_address(addr);
}

std::string
get_bind_address() {
  return manager->get_bind_address();
}

void
set_bind_address(const std::string& addr) {
  if (addr == manager->get_bind_address())
    return;

  if (manager->listen()->is_open())
    throw client_error("torrent::set_bind(...) called, but listening socket is open");

  manager->set_bind_address(addr);
}

uint16_t
get_listen_port() {
  return manager->listen()->get_port();
}

uint32_t
get_total_handshakes() {
  return manager->handshake_manager()->get_size();
}

int64_t
get_next_timeout() {
  Timer::update();

  return !taskScheduler.empty() ?
    std::max(taskScheduler.get_next_timeout() - Timer::cache(), Timer()).usec() :
    60 * 1000000;
}

int
get_down_throttle() {
  return std::max(throttleRead.get_quota(), 0);
}

void
set_down_throttle(int bytes) {
  throttleRead.set_quota(bytes > 0 ? bytes : ThrottlePeer::UNLIMITED);
}

int
get_up_throttle() {
  return std::max(throttleWrite.get_quota(), 0);
}

void
set_up_throttle(int bytes) {
  throttleWrite.set_quota(bytes > 0 ? bytes : ThrottlePeer::UNLIMITED);
}

void
set_throttle_interval(uint32_t usec) {
  if (usec <= 0 || usec > 5 * 1000000)
    throw input_error("torrent::set_throttle_interval(...) received an invalid value");

  throttleRead.set_interval(usec);
  throttleWrite.set_interval(usec);
}

const Rate&
get_down_rate() {
  return throttleRead.get_rate_slow();
}

const Rate&
get_up_rate() {
  return throttleWrite.get_rate_slow();
}

char*
get_version() {
  return VERSION;
}

uint32_t
get_hash_read_ahead() {
  return manager->hash_queue()->get_read_ahead();
}

void
set_hash_read_ahead(uint32_t bytes) {
  if (bytes < 64 << 20)
    manager->hash_queue()->set_read_ahead(bytes);
}

uint32_t
get_hash_interval() {
  return manager->hash_queue()->get_interval();
}

void
set_hash_interval(uint32_t usec) {
  if (usec < 1000000)
    manager->hash_queue()->set_interval(usec);
}

uint32_t
get_hash_max_tries() {
  return manager->hash_queue()->get_max_tries();
}

void
set_hash_max_tries(uint32_t tries) {
  if (tries < 100)
    manager->hash_queue()->set_max_tries(tries);
}  

uint32_t
get_open_files() {
  return manager->file_manager()->open_size();
}

uint32_t
get_max_open_files() {
  return manager->file_manager()->get_max_size();
}

void
set_max_open_files(uint32_t size) {
  manager->file_manager()->set_max_size(size);
}

uint32_t
get_open_sockets() {
  return socketManager.size();
}

uint32_t
get_max_open_sockets() {
  return socketManager.max_size();
}

void
set_max_open_sockets(uint32_t size) {
  socketManager.set_max_size(size);
}

Download
download_add(std::istream* s) {
  if (!s->good())
    throw input_error("Could not create download, the input stream is not valid");

  std::auto_ptr<DownloadWrapper> d(new DownloadWrapper);

  *s >> d->get_bencode();

  if (s->fail())
    // Make it configurable whetever we throw or return .end()?
    throw input_error("Could not create download, failed to parse the bencoded data");
  
  parse_main(d->get_bencode(), d.get());
  parse_info(d->get_bencode()["info"], *d->main()->content());

  d->initialize(d->get_bencode()["info"].compute_sha1(),
		PEER_NAME + random_string(20 - std::string(PEER_NAME).size()),
		manager->get_local_address());

  d->set_handshake_manager(manager->handshake_manager());
  d->set_hash_queue(manager->hash_queue());
  d->set_file_manager(manager->file_manager());

  // Default PeerConnection factory functions.
  d->main()->connection_list()->slot_new_connection(&createPeerConnectionDefault);

  parse_tracker(d->get_bencode(), d->main()->tracker_manager());

  // Consider move as much as possible into this function
  // call. Anything that won't cause possible torrent creation errors
  // go in there.
  manager->initialize_download(d.get());

  return Download(d.release());
}

void
download_remove(const std::string& infohash) {
  DownloadManager::iterator itr = manager->download_manager()->find(infohash);

  manager->cleanup_download(*itr);
}

// Add all downloads to dlist. Make sure it's cleared.
void
download_list(DList& dlist) {
  for (DownloadManager::const_iterator itr = manager->download_manager()->begin();
       itr != manager->download_manager()->end(); ++itr)
    dlist.push_back(Download(*itr));
}

// Make sure you check that it's valid.
Download
download_find(const std::string& infohash) {
  return *manager->download_manager()->find(infohash);
}

}
