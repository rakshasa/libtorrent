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
#include <sstream>
#include <memory>
#include <unistd.h>
#include <sigc++/bind.h>
#include <rak/functional.h>

#include "exceptions.h"
#include "torrent.h"
#include "bencode.h"

#include "utils/sha1.h"
#include "utils/string_manip.h"
#include "utils/task_scheduler.h"
#include "utils/throttle.h"
#include "net/listen.h"
#include "net/handshake_manager.h"
#include "net/manager.h"
#include "parse/parse.h"
#include "peer/peer_factory.h"
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

// New API.
class Torrent {
public:
  Torrent() {}

  std::string         m_address;
  std::string         m_bindAddress;

  Listen              m_listen;
  HashQueue           m_hashQueue;
  HandshakeManager    m_handshakeManager;
  DownloadManager     m_downloadManager;

  FileManager         m_fileManager;
};

Torrent* torrent = NULL;

// Find some better way of doing this, or rather... move it outside.
std::string
download_id(const std::string& hash) {
  DownloadManager::iterator itr = torrent->m_downloadManager.find(hash);

  return itr != torrent->m_downloadManager.end() &&
    (*itr)->get_main().is_active() &&
    (*itr)->get_main().is_checked() ?
    (*itr)->get_main().get_me().get_id() : "";
}

void
receive_connection(SocketFd fd, const std::string& hash, const PeerInfo& peer) {
  DownloadManager::iterator itr = torrent->m_downloadManager.find(hash);
  
  if (itr == torrent->m_downloadManager.end() ||
      !(*itr)->get_main().is_active() ||
      !(*itr)->get_main().get_net().add_connection(fd, peer))
    socketManager.close(fd);
}

std::string
bencode_hash(Bencode& b) {
  std::stringstream str;
  str << b;

  if (str.fail())
    throw bencode_error("Could not write bencode to stream");

  std::string s = str.str();
  Sha1 sha1;

  sha1.init();
  sha1.update(s.c_str(), s.size());

  return sha1.final();
}  

void
initialize(Poll* poll) {
  if (torrent != NULL)
    throw client_error("torrent::initialize(...) called but the library has already been initialized");

  Timer::update();

  torrent = new Torrent;
  torrent->m_listen.slot_incoming(sigc::mem_fun(torrent->m_handshakeManager, &HandshakeManager::add_incoming));

  throttleRead.start();
  throttleWrite.start();

  torrent->m_handshakeManager.slot_connected(sigc::ptr_fun3(&receive_connection));
  torrent->m_handshakeManager.slot_download_id(sigc::ptr_fun1(download_id));

  if ((int32_t)poll->max_open_sockets() < 256 + 32)
    throw client_error("Could not initialize libtorrent, poll has too low max_open_sockets");

  pollCustom = poll;

  // By default we reserve 128 fd's for the client.
  socketManager.set_max_size(sysconf(_SC_OPEN_MAX) - 256);
  torrent->m_fileManager.set_max_size(128);
}

// Clean up and close stuff. Stopping all torrents and waiting for
// them to finish is not required, but recommended.
void
cleanup() {
  if (torrent == NULL)
    throw client_error("torrent::cleanup() called but the library is not initialized");

  throttleRead.stop();
  throttleWrite.stop();

  torrent->m_handshakeManager.clear();
  torrent->m_downloadManager.clear();

  delete torrent;
  torrent = NULL;

  pollCustom = NULL;
}

bool
listen_open(uint16_t begin, uint16_t end) {
  if (torrent == NULL)
    throw client_error("listen_open called but the library has not been initialized");

  SocketAddress sa;

  if (!torrent->m_bindAddress.empty() && !sa.set_address(torrent->m_bindAddress))
    throw local_error("Could not parse the ip address to bind");

  if (!torrent->m_listen.open(begin, end, sa))
    return false;

  torrent->m_handshakeManager.set_bind_address(sa);

  for (DownloadManager::const_iterator itr = torrent->m_downloadManager.begin(), last = torrent->m_downloadManager.end();
       itr != last; ++itr)
    (*itr)->get_main().get_me().get_socket_address().set_port(torrent->m_listen.get_port());

  return true;
}

void
listen_close() {
  torrent->m_listen.close();
}

void
perform() {
  Timer::update();
  taskScheduler.execute(Timer::cache());
}

bool
is_inactive() {
  return torrent == NULL ||
    std::find_if(torrent->m_downloadManager.begin(), torrent->m_downloadManager.end(),
		      std::not1(std::mem_fun(&DownloadWrapper::is_stopped)))
    == torrent->m_downloadManager.end();
}

const std::string&
get_address() {
  return torrent->m_address;
}

void
set_address(const std::string& addr) {
  if (addr == torrent->m_address)
    return;

  torrent->m_address = addr;

  for (DownloadManager::const_iterator itr = torrent->m_downloadManager.begin(), last = torrent->m_downloadManager.end();
       itr != last; ++itr)
    (*itr)->get_main().get_me().get_socket_address().set_address(torrent->m_address);
}

const std::string&
get_bind_address() {
  return torrent->m_bindAddress;
}

void
set_bind_address(const std::string& addr) {
  if (addr == torrent->m_bindAddress)
    return;

  if (torrent->m_listen.is_open())
    throw client_error("torrent::set_bind(...) called, but listening socket is open");

  torrent->m_bindAddress = addr;
}

uint16_t
get_listen_port() {
  return torrent->m_listen.get_port();
}

uint32_t
get_total_handshakes() {
  return torrent->m_handshakeManager.get_size();
}

int64_t
get_next_timeout() {
  Timer::update();

  return !taskScheduler.empty() ?
    std::max(taskScheduler.get_next_timeout() - Timer::cache(), Timer()).usec() :
    60 * 1000000;
}

int
get_read_throttle() {
  return std::max(throttleRead.get_quota(), 0);
}

void
set_read_throttle(int bytes) {
  throttleRead.set_quota(bytes > 0 ? bytes : ThrottlePeer::UNLIMITED);
}

int
get_write_throttle() {
  return std::max(throttleWrite.get_quota(), 0);
}

void
set_write_throttle(int bytes) {
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
get_read_rate() {
  return throttleRead.get_rate_slow();
}

const Rate&
get_write_rate() {
  return throttleWrite.get_rate_slow();
}

char*
get_version() {
  return VERSION;
}

uint32_t
get_hash_read_ahead() {
  return torrent->m_hashQueue.get_read_ahead();
}

void
set_hash_read_ahead(uint32_t bytes) {
  if (bytes < 64 << 20)
    torrent->m_hashQueue.set_read_ahead(bytes);
}

uint32_t
get_hash_interval() {
  return torrent->m_hashQueue.get_interval();
}

void
set_hash_interval(uint32_t usec) {
  if (usec < 1000000)
    torrent->m_hashQueue.set_interval(usec);
}

uint32_t
get_hash_max_tries() {
  return torrent->m_hashQueue.get_max_tries();
}

void
set_hash_max_tries(uint32_t tries) {
  if (tries < 100)
    torrent->m_hashQueue.set_max_tries(tries);
}  

uint32_t
get_max_open_files() {
  return torrent->m_fileManager.max_size();
}

void
set_max_open_files(uint32_t size) {
  torrent->m_fileManager.set_max_size(size);
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
  
  d->get_main().get_me().get_socket_address().set_address(torrent->m_address);
  d->get_main().get_me().get_socket_address().set_port(torrent->m_listen.get_port());

  parse_main(d->get_bencode(), d->get_main());
  parse_info(d->get_bencode()["info"], d->get_main().get_state().get_content());

  d->initialize(bencode_hash(d->get_bencode()["info"]),
		PEER_NAME + random_string(20 - std::string(PEER_NAME).size()));

  d->set_handshake_manager(&torrent->m_handshakeManager);
  d->set_hash_queue(&torrent->m_hashQueue);
  d->set_file_manager(&torrent->m_fileManager);

  // Default PeerConnection factory functions.
  d->get_main().get_net().slot_create_connection(sigc::bind(sigc::ptr_fun(createPeerConnectionDefault),
							    &d->get_main().get_state(),
							    &d->get_main().get_net()));

  parse_tracker(d->get_bencode(), &d->get_main().get_tracker());

  torrent->m_downloadManager.insert(d.get());

  return Download(d.release());
}

void
download_remove(const std::string& infohash) {
  torrent->m_downloadManager.erase(torrent->m_downloadManager.find(infohash));
}

// Add all downloads to dlist. Make sure it's cleared.
void
download_list(DList& dlist) {
  for (DownloadManager::const_iterator itr = torrent->m_downloadManager.begin();
       itr != torrent->m_downloadManager.end(); ++itr)
    dlist.push_back(Download(*itr));
}

// Make sure you check that it's valid.
Download
download_find(const std::string& infohash) {
  return *torrent->m_downloadManager.find(infohash);
}

}
