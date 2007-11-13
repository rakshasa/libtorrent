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

#ifndef LIBTORRENT_DOWNLOAD_WRAPPER_H
#define LIBTORRENT_DOWNLOAD_WRAPPER_H

#include <rak/socket_address.h>
#include <sigc++/connection.h>
#include <sigc++/signal.h>

#include "data/chunk_handle.h"
#include "download/download_info.h"

#include "download_main.h"

namespace torrent {

// Remember to clean up the pointers, DownloadWrapper won't do it.

class AddressList;
class FileManager;
class HashQueue;
class HashTorrent;
class HandshakeManager;
class DownloadInfo;
class Object;
class Peer;

class DownloadWrapper {
public:
  typedef sigc::signal0<void>                     Signal;
  typedef sigc::signal1<void, uint32_t>           SignalChunk;
  typedef sigc::signal1<void, const std::string&> SignalString;
  typedef sigc::signal1<void, Peer*>              SignalPeer;

  DownloadWrapper();
  ~DownloadWrapper();

  // Initialize hash checker and various download stuff.
  void                initialize(const std::string& hash, const std::string& id);

  void                open();
  void                close();

//   bool                is_open() const                         { return m_main.is_open(); }
  bool                is_stopped() const;

  DownloadMain*       main()                                  { return &m_main; }
  const DownloadMain* main() const                            { return &m_main; }
  HashTorrent*        hash_checker()                          { return m_hashChecker; }

  Object*             bencode()                               { return m_bencode; }
  void                set_bencode(Object* o)                  { m_bencode = o; }

  HashQueue*          hash_queue()                            { return m_hashQueue; }
  void                set_hash_queue(HashQueue* q)            { m_hashQueue = q; }

  const std::string&  complete_hash()                            { return m_hash; }
  const char*         chunk_hash(unsigned int index)             { return m_hash.c_str() + 20 * index; }
  void                set_complete_hash(const std::string& hash) { m_hash = hash; }

  DownloadInfo*       info()                                  { return m_main.info(); }

  int                 connection_type() const                 { return m_connectionType; }
  void                set_connection_type(int t)              { m_connectionType = t; }

  void                receive_initial_hash();
  void                receive_hash_done(ChunkHandle handle, const char* hash);

  void                check_chunk_hash(ChunkHandle handle);

  void                receive_storage_error(const std::string& str);
  void                receive_tracker_success(AddressList* l);
  void                receive_tracker_failed(const std::string& msg);

  void                receive_peer_connected(PeerConnectionBase* peer);
  void                receive_peer_disconnected(PeerConnectionBase* peer);

  void                receive_tick(uint32_t ticks);

  void                receive_update_priorities();

  Signal&             signal_initial_hash()          { return m_signalInitialHash; }
  Signal&             signal_download_done()         { return m_signalDownloadDone; }

  // The list of addresses is guaranteed to be sorted and unique.
  Signal&             signal_tracker_success()       { return m_signalTrackerSuccess; }
  SignalString&       signal_tracker_failed()        { return m_signalTrackerFailed; }

  SignalChunk&        signal_chunk_passed()          { return m_signalChunkPassed; }
  SignalChunk&        signal_chunk_failed()          { return m_signalChunkFailed; }

  SignalPeer&         signal_peer_connected()        { return m_signalPeerConnected; }
  SignalPeer&         signal_peer_disconnected()     { return m_signalPeerDisconnected; }

private:
  DownloadWrapper(const DownloadWrapper&);
  void operator = (const DownloadWrapper&);

  void                finished_download();

  DownloadMain        m_main;
  Object*             m_bencode;
  HashTorrent*        m_hashChecker;
  HashQueue*          m_hashQueue;

  std::string         m_hash;

  int                 m_connectionType;

  rak::priority_item  m_delayDownloadDone;

  Signal              m_signalInitialHash;
  Signal              m_signalDownloadDone;

  Signal              m_signalTrackerSuccess;
  SignalString        m_signalTrackerFailed;

  SignalChunk         m_signalChunkPassed;
  SignalChunk         m_signalChunkFailed;

  sigc::connection    m_connectionChunkPassed;
  sigc::connection    m_connectionChunkFailed;

  SignalPeer          m_signalPeerConnected;
  SignalPeer          m_signalPeerDisconnected;
};

}

#endif
