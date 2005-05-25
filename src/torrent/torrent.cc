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
// Contact:  Jari Sundell <jaris@ifi.uio.no>
//
//           Skomakerveien 33
//           3185 Skoppum, NORWAY

#include "config.h"

#include <iostream>
#include <sstream>
#include <memory>
#include <unistd.h>
#include <algo/algo.h>
#include <sigc++/bind.h>

#include "exceptions.h"
#include "torrent.h"
#include "bencode.h"

#include "utils/sha1.h"
#include "utils/string_manip.h"
#include "utils/task_schedule.h"
#include "utils/throttle.h"
#include "net/listen.h"
#include "net/handshake_manager.h"
#include "net/poll.h"
#include "net/poll_select.h"
#include "parse/parse.h"
#include "data/hash_queue.h"
#include "data/hash_torrent.h"
#include "download/download_manager.h"
#include "download/download_wrapper.h"

using namespace algo;

namespace torrent {

int64_t Timer::m_cache;

Listen* listen = NULL;
HashQueue* hashQueue = NULL;
HandshakeManager* handshakes = NULL;
DownloadManager* downloadManager = NULL;

ThrottlePeer throttleRead;
ThrottlePeer throttleWrite;

// Find some better way of doing this, or rather... move it outside.
std::string
download_id(const std::string& hash) {
  DownloadWrapper* d = downloadManager->find(hash);

  return d &&
    d->get_main().is_active() &&
    d->get_main().is_checked() ?
    d->get_main().get_me().get_id() : "";
}

void
receive_connection(SocketFd fd, const std::string& hash, const PeerInfo& peer) {
  DownloadWrapper* d = downloadManager->find(hash);
  
  if (!d ||
      !d->get_main().is_active() ||
      !d->get_main().is_checked() ||
      !d->get_main().get_net().add_connection(fd, peer))
    fd.close();
}

void
initialize() {
  if (listen || hashQueue || handshakes || downloadManager)
    throw client_error("torrent::initialize() called but the library has already been initialized");

  Timer::update();

  listen = new Listen;
  hashQueue = new HashQueue;
  handshakes = new HandshakeManager;
  downloadManager = new DownloadManager;

  listen->slot_incoming(sigc::mem_fun(*handshakes, &HandshakeManager::add_incoming));

  throttleRead.start();
  throttleWrite.start();

  handshakes->slot_connected(sigc::ptr_fun3(&receive_connection));
  handshakes->slot_download_id(sigc::ptr_fun1(download_id));

  Poll::set_open_max(sysconf(_SC_OPEN_MAX));
}

// Clean up and close stuff. Stopping all torrents and waiting for
// them to finish is not required, but recommended.
void
cleanup() {
  if (listen == NULL || hashQueue == NULL || handshakes == NULL || downloadManager == NULL)
    throw client_error("torrent::cleanup() called but the library is not initialized");

  throttleRead.stop();
  throttleWrite.stop();

  handshakes->clear();
  downloadManager->clear();

  delete listen;
  delete hashQueue;
  delete handshakes;
  delete downloadManager;

  listen = NULL;
  hashQueue = NULL;
  handshakes = NULL;
  downloadManager = NULL;
}

bool
listen_open(uint16_t begin, uint16_t end, const std::string& addr) {
  if (listen == NULL)
    throw client_error("listen_open called but the library has not been initialized");

  SocketAddress sa;

  if (!addr.empty() && !sa.set_address(addr))
    throw local_error("Could not parse the ip address to bind");

  if (!listen->open(begin, end, sa))
    return false;

  handshakes->set_bind_address(sa);

  std::for_each(downloadManager->get_list().begin(), downloadManager->get_list().end(),
		call_member(call_member(&DownloadWrapper::get_main),
			    &DownloadMain::set_port,
			    value(listen->get_port())));

  return true;
}

void
listen_close() {
  listen->close();
}

// Set the file descriptors we want to pool for R/W/E events. All
// fd_set's must be valid pointers. Returns the highest fd.
void
mark(fd_set* readSet, fd_set* writeSet, fd_set* exceptSet, int* maxFd) {
  if (readSet == NULL || writeSet == NULL || exceptSet == NULL || maxFd == NULL)
    throw client_error("torrent::mark(...) received a NULL pointer");

  *maxFd = 0;

  Poll::read_set().prepare();
  std::for_each(Poll::read_set().begin(), Poll::read_set().end(), poll_mark(readSet, maxFd));

  Poll::write_set().prepare();
  std::for_each(Poll::write_set().begin(), Poll::write_set().end(), poll_mark(writeSet, maxFd));
  
  Poll::except_set().prepare();
  std::for_each(Poll::except_set().begin(), Poll::except_set().end(), poll_mark(exceptSet, maxFd));
}

// Do work on the polled file descriptors.
void
work(fd_set* readSet, fd_set* writeSet, fd_set* exceptSet, int maxFd) {
  // Update the cached time.
  Timer::update();

  if (readSet == NULL || writeSet == NULL || exceptSet == NULL)
    throw client_error("Torrent::work(...) received a NULL pointer");

  // Make sure we don't do read/write on fd's that are in except. This should
  // not be a problem as any except call should remove it from the m_*Set's.

  Poll::except_set().prepare();
  std::for_each(Poll::except_set().begin(), Poll::except_set().end(),
		poll_check(exceptSet, std::mem_fun(&SocketBase::except)));

  Poll::read_set().prepare();
  std::for_each(Poll::read_set().begin(), Poll::read_set().end(),
		poll_check(readSet, std::mem_fun(&SocketBase::read)));

  Poll::write_set().prepare();
  std::for_each(Poll::write_set().begin(), Poll::write_set().end(),
		poll_check(writeSet, std::mem_fun(&SocketBase::write)));

  // TODO: Consider moving before the r/w/e. libsic++ should remove the use of
  // zero timeout stuff to send signal. Better yet, use on both sides, it's cheap.
  TaskSchedule::perform(Timer::current());
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

Download
download_create(std::istream* s) {
  if (s == NULL)
    throw client_error("torrent::download_create(...) received a NULL pointer");

  if (!s->good())
    throw input_error("Could not create download, the input stream is not valid");

  std::auto_ptr<DownloadWrapper> d(new DownloadWrapper);

  *s >> d->get_bencode();

  if (s->fail())
    // Make it configurable whetever we throw or return .end()?
    throw input_error("Could not create download, failed to parse the bencoded data");
  
  d->get_main().set_port(listen->get_port());

  parse_main(d->get_bencode(), d->get_main());
  parse_info(d->get_bencode()["info"], d->get_main().get_state().get_content());

  d->initialize(bencode_hash(d->get_bencode()["info"]),
		Settings::peerName + random_string(20 - Settings::peerName.size()));

  d->set_handshake_manager(handshakes);
  d->set_hash_queue(hashQueue);

  parse_tracker(d->get_bencode(), &d->get_main().get_tracker());

  downloadManager->add(d.get());

  return Download(d.release());
}

// Add all downloads to dlist. Make sure it's cleared.
void
download_list(DList& dlist) {
  for (DownloadManager::DownloadList::const_iterator itr = downloadManager->get_list().begin();
       itr != downloadManager->get_list().end(); ++itr)
    dlist.push_back(Download(*itr));
}

// Make sure you check that it's valid.
Download
download_find(const std::string& id) {
  return downloadManager->find(id);
}

void
download_remove(const std::string& id) {
  downloadManager->remove(id);
}

Bencode&
download_bencode(const std::string& id) {
  DownloadWrapper* d = downloadManager->find(id);

  if (d == NULL)
    throw client_error("Tried to call download_bencode(id) with non-existing download");

  return d->get_bencode();
}

// Throws a local_error of some sort.
int64_t
get(GValue t) {
  switch (t) {
  case LISTEN_PORT:
    return listen->get_port();

  case HANDSHAKES_TOTAL:
    return handshakes->get_size();

  case SHUTDOWN_DONE:
    return std::find_if(downloadManager->get_list().begin(), downloadManager->get_list().end(),
			bool_not(call_member(call_member(&DownloadWrapper::get_main),
					     &DownloadMain::is_stopped)))
      == downloadManager->get_list().end();

  case FILES_CHECK_WAIT:
    return Settings::filesCheckWait;

  case DEFAULT_PEERS_MIN:
    return DownloadSettings::global().minPeers;

  case DEFAULT_PEERS_MAX:
    return DownloadSettings::global().maxPeers;

  case DEFAULT_CHOKE_CYCLE:
    return DownloadSettings::global().chokeCycle;

  case TIME_CURRENT:
    return Timer::current().usec();

  case TIME_SELECT:
    return TaskSchedule::get_timeout().usec();

  case THROTTLE_ROOT_CONST_RATE:
    return std::max(throttleWrite.get_quota(), 0);

  case THROTTLE_READ_CONST_RATE:
    return std::max(throttleRead.get_quota(), 0);

  default:
    throw internal_error("get(GValue) received invalid type");
  }
}

std::string
get(GString t) {
  std::string s;

  switch (t) {
  case LIBRARY_NAME:
    return std::string("libtorrent") + " " VERSION;

  default:
    throw internal_error("get(GString) received invalid type");
  }
}

void
set(GValue t, int64_t v) {
  switch (t) {
  case FILES_CHECK_WAIT:
    if (v >= 0 && v < 60 * 1000000)
      Settings::filesCheckWait = v;
    break;

  case DEFAULT_PEERS_MIN:
    if (v > 0 && v < 1000)
      DownloadSettings::global().minPeers = v;
    break;

  case DEFAULT_PEERS_MAX:
    if (v > 0 && v < 1000)
      DownloadSettings::global().maxPeers = v;
    break;

  case DEFAULT_CHOKE_CYCLE:
    if (v > 10 * 1000000 && v < 3600 * 1000000)
      DownloadSettings::global().chokeCycle = v;
    break;

  case THROTTLE_ROOT_CONST_RATE:
    throttleWrite.set_quota(v > 0 ? v : ThrottlePeer::UNLIMITED);
    break;

  case THROTTLE_READ_CONST_RATE:
    throttleRead.set_quota(v > 0 ? v : ThrottlePeer::UNLIMITED);
    break;

  default:
    throw internal_error("set(GValue, int) received invalid type");
  }
}

void
set(GString t, const std::string& s) {
}

}
