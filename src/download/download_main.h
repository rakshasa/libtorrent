#ifndef LIBTORRENT_DOWNLOAD_MAIN_H
#define LIBTORRENT_DOWNLOAD_MAIN_H

#include <deque>
#include <utility>

#include "data/chunk_handle.h"
#include "download/delegator.h"
#include "net/address_list.h"
#include "net/data_buffer.h"
#include "torrent/data/file_list.h"
#include "torrent/download/group_entry.h"
#include "torrent/peer/peer_list.h"
#include "torrent/tracker/wrappers.h"
#include "torrent/utils/scheduler.h"

namespace torrent {

class ChunkList;
class ChunkSelector;
class ChunkStatistics;

class choke_group;
class ConnectionList;
class DownloadWrapper;
class HandshakeManager;
class DownloadInfo;
class ThrottleList;
class InitialSeeding;

class DownloadMain {
public:
  using have_queue_type = std::deque<std::pair<std::chrono::microseconds, uint32_t>>;
  using pex_list        = std::vector<SocketAddressCompact>;

  DownloadMain();
  ~DownloadMain();

  DownloadMain(const DownloadMain&) = delete;
  DownloadMain& operator=(const DownloadMain&) = delete;

  void                post_initialize();

  void                open(int flags);
  void                close();

  void                start(int flags);
  void                stop();

  class choke_group*       choke_group()                           { return m_choke_group; }
  const class choke_group* c_choke_group() const                   { return m_choke_group; }
  void                     set_choke_group(class choke_group* grp) { m_choke_group = grp; }

  tracker::TrackerControllerWrapper tracker_controller()           { return m_tracker_controller; }
  TrackerList*                      tracker_list()                 { return m_tracker_list; }

  DownloadInfo*       info()                                     { return m_info; }

  // Only retrieve writable chunks when the download is active.
  ChunkList*          chunk_list()                               { return m_chunkList; }
  ChunkSelector*      chunk_selector()                           { return m_chunkSelector; }
  ChunkStatistics*    chunk_statistics()                         { return m_chunkStatistics; }

  Delegator*          delegator()                                { return &m_delegator; }

  have_queue_type*    have_queue()                               { return &m_haveQueue; }

  InitialSeeding*     initial_seeding()                          { return m_initial_seeding.get(); }
  bool                start_initial_seeding();
  void                initial_seeding_done(PeerConnectionBase* pcb);

  ConnectionList*     connection_list()                          { return m_connectionList; }
  FileList*           file_list()                                { return &m_fileList; }
  PeerList*           peer_list()                                { return &m_peerList; }

  std::pair<ThrottleList*, ThrottleList*> throttles(const sockaddr* sa);

  ThrottleList*       upload_throttle()                          { return m_upload_throttle; }
  void                set_upload_throttle(ThrottleList* t)       { m_upload_throttle = t; }

  ThrottleList*       download_throttle()                        { return m_download_throttle; }
  void                set_download_throttle(ThrottleList* t)     { m_download_throttle = t; }

  group_entry*        up_group_entry()                           { return &m_up_group_entry; }
  group_entry*        down_group_entry()                         { return &m_down_group_entry; }

  DataBuffer          get_ut_pex(bool initial)                   { return (initial ? m_ut_pex_initial : m_ut_pex_delta).clone(); }

  bool                want_pex_msg();

  void                set_metadata_size(size_t s);

  // Carefull with these.
  void                setup_delegator();
  void                setup_tracker();

  using slot_count_handshakes_type = std::function<uint32_t(DownloadMain*)>;
  using slot_hash_check_add_type   = std::function<void(ChunkHandle)>;

  using slot_start_handshake_type = std::function<void(const rak::socket_address&, DownloadMain*)>;
  using slot_stop_handshakes_type = std::function<void(DownloadMain*)>;

  void                slot_start_handshake(slot_start_handshake_type s) { m_slot_start_handshake = std::move(s); }
  void                slot_stop_handshakes(slot_stop_handshakes_type s) { m_slot_stop_handshakes = std::move(s); }
  void                slot_count_handshakes(slot_count_handshakes_type s) { m_slot_count_handshakes = std::move(s); }
  void                slot_hash_check_add(slot_hash_check_add_type s)     { m_slot_hash_check_add = std::move(s); }

  void                add_peer(const rak::socket_address& sa);

  void                receive_connect_peers();
  void                receive_chunk_done(unsigned int index);
  void                receive_corrupt_chunk(PeerInfo* peerInfo);

  void                receive_tracker_success();
  void                receive_tracker_request();

  void                receive_do_peer_exchange();

  void                do_peer_exchange();

  void                update_endgame();

  auto&               delay_download_done()       { return m_delay_download_done; }
  auto&               delay_partially_done()      { return m_delay_partially_done; }
  auto&               delay_partially_restarted() { return m_delay_partially_restarted; }

  auto&               delay_disconnect_peers()    { return m_delay_disconnect_peers; }

private:
  void                setup_start();
  void                setup_stop();

  DownloadInfo*       m_info;

  tracker::TrackerControllerWrapper m_tracker_controller;
  TrackerList*                      m_tracker_list;

  class choke_group*  m_choke_group{};

  group_entry         m_up_group_entry;
  group_entry         m_down_group_entry;

  ChunkList*          m_chunkList;
  ChunkSelector*      m_chunkSelector;
  ChunkStatistics*    m_chunkStatistics;

  Delegator           m_delegator;
  have_queue_type     m_haveQueue;
  std::unique_ptr<InitialSeeding> m_initial_seeding;

  ConnectionList*     m_connectionList;
  FileList            m_fileList;
  PeerList            m_peerList;

  DataBuffer          m_ut_pex_delta;
  DataBuffer          m_ut_pex_initial;
  pex_list            m_ut_pex_list;

  ThrottleList*       m_upload_throttle{};
  ThrottleList*       m_download_throttle{};

  slot_start_handshake_type  m_slot_start_handshake;
  slot_stop_handshakes_type  m_slot_stop_handshakes;

  slot_count_handshakes_type m_slot_count_handshakes;
  slot_hash_check_add_type   m_slot_hash_check_add;

  utils::SchedulerEntry      m_delay_download_done;
  utils::SchedulerEntry      m_delay_partially_done;
  utils::SchedulerEntry      m_delay_partially_restarted;

  utils::SchedulerEntry      m_delay_disconnect_peers;
  utils::SchedulerEntry      m_task_tracker_request;
};

} // namespace torrent

#endif
