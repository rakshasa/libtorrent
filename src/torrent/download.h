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

#ifndef LIBTORRENT_DOWNLOAD_H
#define LIBTORRENT_DOWNLOAD_H

#include <torrent/common.h>
#include <torrent/entry.h>
#include <torrent/peer.h>
#include <torrent/tracker.h>

#include <iosfwd>
#include <list>
#include <vector>
#include <sigc++/slot.h>
#include <sigc++/connection.h>

namespace torrent {

typedef std::list<Peer> PList;

class Bencode;
struct DownloadWrapper;

class Download {
public:
  typedef std::vector<uint16_t> SeenVector;

  enum {
    NUMWANT_DISABLED = -1
  };

  Download(DownloadWrapper* d = NULL) : m_ptr(d) {}

  // Not active atm. Opens and prepares/closes the files.
  void                 open();
  void                 close();

  // Torrent must be open for calls to hash_check(bool) and
  // hash_resume_save(). hash_resume_clear() removes resume data from
  // the bencode'ed torrent.
  void                 hash_check(bool resume = true);
  void                 hash_resume_save();
  void                 hash_resume_clear();

  // Start/stop the download. The torrent must be open.
  void                 start();
  void                 stop();

  // Does not check if the download has been removed.
  bool                 is_valid()  { return m_ptr; }

  bool                 is_open();
  bool                 is_active();
  bool                 is_tracker_busy();

  bool                 is_hash_checked();
  bool                 is_hash_checking();

  // Returns "" if the object is not valid.
  std::string          get_name();
  std::string          get_hash();
  std::string          get_id();

  // Unix epoche, 0 == unknown.
  uint32_t             get_creation_date();

  // Only set the root directory while the torrent is closed.
  std::string          get_root_dir();
  void                 set_root_dir(const std::string& dir);

  // Bytes uploaded this session.
  uint64_t             get_bytes_up();
  // Bytes downloaded this session.
  uint64_t             get_bytes_down();
  // Bytes completed.
  uint64_t             get_bytes_done();
  // Size of the torrent.
  uint64_t             get_bytes_total();

  uint32_t             get_chunks_size();
  uint32_t             get_chunks_done();
  uint32_t             get_chunks_total();

  // Bytes per second.
  uint32_t             get_rate_up();
  uint32_t             get_rate_down();
  
  const unsigned char* get_bitfield_data();
  uint32_t             get_bitfield_size();

  uint32_t             get_peers_min();
  uint32_t             get_peers_max();
  uint32_t             get_peers_connected();
  uint32_t             get_peers_not_connected();

  uint32_t             get_uploads_max();
  
  uint64_t             get_tracker_timeout();
  int16_t              get_tracker_numwant();

  void                 set_peers_min(uint32_t v);
  void                 set_peers_max(uint32_t v);

  void                 set_uploads_max(uint32_t v);

  void                 set_tracker_timeout(uint64_t v, bool force = false);
  void                 set_tracker_numwant(int16_t n);

  // Access the trackers in the torrent.
  Tracker              get_tracker(uint32_t index);
  uint32_t             get_tracker_size();

  void                 cycle_tracker_group(int group);

  // Access the files in the torrent.
  Entry                get_entry(uint32_t index);
  uint32_t             get_entry_size();

  const SeenVector&    get_seen();

  // Call this when you want the modifications of the download priorities
  // in the entries to take effect. It is slightly expensive as it rechecks
  // all the peer bitfields to see if we are still interested.
  void                 update_priorities();

  // If you create a peer list, you *must* keep it up to date with the signals
  // peer_{connected,disconnected}. Otherwise you may experience undefined
  // behaviour when using invalid peers in the list.
  void                 peer_list(PList& pList);
  Peer                 peer_find(const std::string& id);

  // Note on signals: If you bind it to a class member function, make sure the
  // class does not get copied as the binding only points to the original
  // memory location.

  typedef sigc::slot0<void>                     SlotVoid;
  typedef sigc::slot1<void, const std::string&> SlotString;

  typedef sigc::slot1<void, Peer>               SlotPeer;
  typedef sigc::slot1<void, std::istream*>      SlotIStream;
  typedef sigc::slot1<void, uint32_t>           SlotChunk;

  sigc::connection    signal_download_done(SlotVoid s);
  sigc::connection    signal_hash_done(SlotVoid s);

  sigc::connection    signal_peer_connected(SlotPeer s);
  sigc::connection    signal_peer_disconnected(SlotPeer s);

  sigc::connection    signal_tracker_succeded(SlotVoid s);
  sigc::connection    signal_tracker_failed(SlotString s);
  sigc::connection    signal_tracker_dump(SlotIStream s);

  sigc::connection    signal_chunk_passed(SlotChunk s);
  sigc::connection    signal_chunk_failed(SlotChunk s);

  // Various network log message signals.
  sigc::connection    signal_network_log(SlotString s);

  // Emits error messages if there are problems opening files for
  // read/write when the download is active. The client should stop
  // the download if it receive any of these as it will not be able to
  // continue.
  sigc::connection    signal_storage_error(SlotString s);

private:
  DownloadWrapper*      m_ptr;
};

}

#endif

