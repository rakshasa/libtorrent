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
class Rate;
class DownloadWrapper;

// Download is safe to copy and destory as it is just a pointer to an
// internal class.

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
  bool                 is_valid() const { return m_ptr; }

  bool                 is_open() const;
  bool                 is_active() const;
  bool                 is_tracker_busy() const;

  bool                 is_hash_checked() const;
  bool                 is_hash_checking() const;

  // Returns "" if the object is not valid.
  std::string          get_name() const;
  std::string          get_hash() const;
  std::string          get_id() const;

  // Unix epoche, 0 == unknown.
  uint32_t             get_creation_date() const;

  Bencode&             get_bencode();
  const Bencode&       get_bencode() const;

  // Only set the root directory while the torrent is closed.
  std::string          get_root_dir() const;
  void                 set_root_dir(const std::string& dir);

  const Rate&          get_down_rate() const;
  const Rate&          get_up_rate() const;

  // Bytes completed.
  uint64_t             get_bytes_done() const;
  // Size of the torrent.
  uint64_t             get_bytes_total() const;

  uint32_t             get_chunks_size() const;
  uint32_t             get_chunks_done() const;
  uint32_t             get_chunks_total() const;

  const unsigned char* get_bitfield_data() const;
  uint32_t             get_bitfield_size() const;

  uint32_t             get_peers_min() const;
  uint32_t             get_peers_max() const;
  uint32_t             get_peers_connected() const;
  uint32_t             get_peers_not_connected() const;

  uint32_t             peers_currently_unchoked() const;
  uint32_t             peers_currently_interested() const;

  uint32_t             get_uploads_max() const;
  
  uint64_t             get_tracker_timeout() const;
  int16_t              get_tracker_numwant() const;

  void                 set_peers_min(uint32_t v);
  void                 set_peers_max(uint32_t v);

  void                 set_uploads_max(uint32_t v);

  void                 set_tracker_numwant(int32_t n);

  // Access the trackers in the torrent.
  Tracker              get_tracker(uint32_t index);
  const Tracker        get_tracker(uint32_t index) const;
  uint32_t             get_tracker_size() const;
  uint32_t             get_tracker_focus() const;

  // Perhaps make tracker_cycle_group part of Tracker?
  void                 tracker_send_completed();
  void                 tracker_cycle_group(int group);
  void                 tracker_manual_request(bool force);

  // Access the files in the torrent.
  Entry                get_entry(uint32_t index);
  uint32_t             get_entry_size() const;

  const SeenVector&    get_seen() const;

  typedef enum {
    CONNECTION_LEECH,
    CONNECTION_SEED
  } ConnectionType;

  ConnectionType       get_connection_type() const;
  void                 set_connection_type(ConnectionType t);

  // Call this when you want the modifications of the download priorities
  // in the entries to take effect. It is slightly expensive as it rechecks
  // all the peer bitfields to see if we are still interested.
  void                 update_priorities();

  // If you create a peer list, you *must* keep it up to date with the signals
  // peer_{connected,disconnected}. Otherwise you may experience undefined
  // behaviour when using invalid peers in the list.
  void                 peer_list(PList& pList);
  Peer                 peer_find(const std::string& id);

  typedef sigc::slot0<void>                     SlotVoid;
  typedef sigc::slot1<void, const std::string&> SlotString;

  typedef sigc::slot1<void, Peer>               SlotPeer;
  typedef sigc::slot1<void, std::istream*>      SlotIStream;
  typedef sigc::slot1<void, uint32_t>           SlotChunk;

  // signal_download_done is a delayed signal so it is safe to
  // stop/close the torrent when received. The signal is only emitted
  // when the torrent is active, so hash checking will not trigger it.
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
  DownloadWrapper*    m_ptr;
};

}

#endif

