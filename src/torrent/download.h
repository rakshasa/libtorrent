// libTorrent - BitTorrent library
// Copyright (C) 2005-2007, Jari Sundell
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

#include <list>
#include <vector>
#include <string>
#include <sigc++/connection.h>
#include <sigc++/functors/slot.h>

#include <torrent/common.h>
#include <torrent/peer/peer.h>

namespace torrent {

typedef std::list<Peer*> PList;

// Download is safe to copy and destory as it is just a pointer to an
// internal class.

class LIBTORRENT_EXPORT Download {
public:
  static const uint32_t numwanted_diabled = ~uint32_t();

  Download(DownloadWrapper* d = NULL) : m_ptr(d) {}

  // Not active atm. Opens and prepares/closes the files.
  void                open();
  void                close();

  // When 'tryQuick' is true, it will only check if the chunks can be
  // mmaped and stops if one is encountered. If it doesn't find any
  // mappable chunks it will return true to indicate that it is
  // finished and a hash done signal has been queued.
  //
  // Chunk ranges that have valid resume data won't be checked.
  bool                hash_check(bool tryQuick);
  void                hash_stop();

  //   static const int start_no_create = (1 << 0);
  static const int start_keep_baseline = (1 << 1);
  static const int start_skip_tracker  = (1 << 2);

  static const int stop_skip_tracker   = (1 << 0);

  // Start/stop the download. The torrent must be open.
  void                start(int flags);
  void                stop(int flags);

  // Does not check if the download has been removed.
  bool                is_valid() const { return m_ptr; }

  bool                is_open() const;
  bool                is_active() const;

  bool                is_hash_checked() const;
  bool                is_hash_checking() const;

  bool                is_private() const;
  bool                is_pex_active() const;
  bool                is_pex_enabled() const;
  void                set_pex_enabled(bool enabled);

  // Returns "" if the object is not valid.
  const std::string&  name() const;

  const HashString&   info_hash() const;
  const HashString&   info_hash_obfuscated() const;
  const HashString&   local_id() const;

  // Unix epoche, 0 == unknown.
  uint32_t            creation_date() const;

  Object*             bencode();
  const Object*       bencode() const;

  TrackerList*        tracker_list() const;

  FileList*           file_list() const;
  PeerList*           peer_list();
  const PeerList*     peer_list() const;
  const TransferList* transfer_list() const;

  // Remove the old non-const versions.
  Rate*               down_rate();
  const Rate*         down_rate() const;
  Rate*               mutable_down_rate();

  Rate*               up_rate();
  const Rate*         up_rate() const;
  Rate*               mutable_up_rate();

  Rate*               skip_rate();
  const Rate*         skip_rate() const;
  Rate*               mutable_skip_rate();

  // Bytes completed.
  uint64_t            bytes_done() const;

  uint32_t            chunks_hashed() const;

  const uint8_t*      chunks_seen() const;

  // Set the number of finished chunks for closed torrents.
  void                set_chunks_done(uint32_t chunks);

  // Use the below to set the resume data and what chunk ranges need
  // to be hash checked. If they arn't called then by default it will
  // use an cleared bitfield and check the whole range.
  //
  // These must be called when is_open, !is_checked and !is_checking.
  void                set_bitfield(bool allSet);
  void                set_bitfield(uint8_t* first, uint8_t* last);
  void                clear_range(uint32_t first, uint32_t last);

  // Temporary hack for syncing chunks to disk before hash resume is
  // saved.
  void                sync_chunks();

  uint32_t            peers_min() const;
  uint32_t            peers_max() const;
  uint32_t            peers_connected() const;
  uint32_t            peers_not_connected() const;
  uint32_t            peers_complete() const;
  uint32_t            peers_accounted() const;

  uint32_t            peers_currently_unchoked() const;
  uint32_t            peers_currently_interested() const;

  uint32_t            size_pex() const;
  uint32_t            max_size_pex() const;

  bool                accepting_new_peers() const;
  uint32_t            uploads_max() const;
  
  void                set_peers_min(uint32_t v);
  void                set_peers_max(uint32_t v);

  void                set_uploads_max(uint32_t v);

  typedef enum {
    CONNECTION_LEECH,
    CONNECTION_SEED
  } ConnectionType;

  ConnectionType      connection_type() const;
  void                set_connection_type(ConnectionType t);

  // Call this when you want the modifications of the download priorities
  // in the entries to take effect. It is slightly expensive as it rechecks
  // all the peer bitfields to see if we are still interested.
  void                update_priorities();

  // If you create a peer list, you *must* keep it up to date with the signals
  // peer_{connected,disconnected}. Otherwise you may experience undefined
  // behaviour when using invalid peers in the list.
  void                peer_list(PList& pList);
  Peer*               peer_find(const std::string& id);

  void                disconnect_peer(Peer* p);

  typedef sigc::slot0<void>                                          slot_void_type;
  typedef sigc::slot1<void, const std::string&>                      slot_string_type;

  typedef sigc::slot1<void, Peer*>                                   slot_peer_type;
  typedef sigc::slot1<void, uint32_t>                                slot_chunk_type;
  typedef sigc::slot3<void, const std::string&, const char*, size_t> slot_dump_type;

  // signal_download_done is a delayed signal so it is safe to
  // stop/close the torrent when received. The signal is only emitted
  // when the torrent is active, so hash checking will not trigger it.
  sigc::connection    signal_download_done(slot_void_type s);
  sigc::connection    signal_hash_done(slot_void_type s);

  sigc::connection    signal_peer_connected(slot_peer_type s);
  sigc::connection    signal_peer_disconnected(slot_peer_type s);

  sigc::connection    signal_tracker_succeded(slot_void_type s);
  sigc::connection    signal_tracker_failed(slot_string_type s);
  sigc::connection    signal_tracker_dump(slot_dump_type s);

  sigc::connection    signal_chunk_passed(slot_chunk_type s);
  sigc::connection    signal_chunk_failed(slot_chunk_type s);

  // Various network log message signals.
  sigc::connection    signal_network_log(slot_string_type s);

  // Emits error messages if there are problems opening files for
  // read/write when the download is active. The client should stop
  // the download if it receive any of these as it will not be able to
  // continue.
  sigc::connection    signal_storage_error(slot_string_type s);

  DownloadWrapper*    ptr() { return m_ptr; }

private:
  DownloadWrapper*    m_ptr;
};

}

#endif
