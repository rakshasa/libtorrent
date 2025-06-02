#ifndef LIBTORRENT_DOWNLOAD_H
#define LIBTORRENT_DOWNLOAD_H

#include <list>
#include <vector>
#include <string>

#include <torrent/common.h>
#include <torrent/peer/peer.h>
#include <torrent/tracker/wrappers.h>

namespace torrent {

class ConnectionList;
class DownloadInfo;
class DownloadMain;
class download_data;

// Download is safe to copy and destory as it is just a pointer to an
// internal class.

class LIBTORRENT_EXPORT Download {
public:
  static constexpr uint32_t numwanted_diabled = ~uint32_t();

  // Start and open flags can be stored in the same integer, same for
  // stop and close flags.
  static constexpr int open_enable_fallocate = (1 << 0);

  static constexpr int start_no_create       = (1 << 1);
  static constexpr int start_keep_baseline   = (1 << 2);
  static constexpr int start_skip_tracker    = (1 << 3);

  static constexpr int stop_skip_tracker     = (1 << 0);

  Download(DownloadWrapper* d = NULL) : m_ptr(d) {}

  const DownloadInfo*  info() const;
  const download_data* data() const;

  // Not active atm. Opens and prepares/closes the files.
  void                open(int flags = 0);
  void                close(int flags = 0);

  // When 'tryQuick' is true, it will only check if the chunks can be
  // mmaped and stops if one is encountered. If it doesn't find any
  // mappable chunks it will return true to indicate that it is
  // finished and a hash done signal has been queued.
  //
  // Chunk ranges that have valid resume data won't be checked.
  bool                hash_check(bool tryQuick);
  void                hash_stop();

  // Start/stop the download. The torrent must be open.
  void                start(int flags = 0);
  void                stop(int flags = 0);

  // Does not check if the download has been removed.
  bool                is_valid() const { return m_ptr; }

  bool                is_hash_checked() const;
  bool                is_hash_checking() const;

  void                set_pex_enabled(bool enabled);

  Object*             bencode();
  const Object*       bencode() const;

  tracker::TrackerControllerWrapper       tracker_controller();
  const tracker::TrackerControllerWrapper c_tracker_controller() const;

  FileList*           file_list() const;
  PeerList*           peer_list();
  const PeerList*     peer_list() const;
  const TransferList* transfer_list() const;

  ConnectionList*       connection_list();
  const ConnectionList* connection_list() const;

  // Bytes completed.
  uint64_t            bytes_done() const;

  uint32_t            chunks_hashed() const;

  const uint8_t*      chunks_seen() const;

  // Set the number of finished chunks for closed torrents.
  void                set_chunks_done(uint32_t chunks_done, uint32_t chunks_wanted);

  // Use the below to set the resume data and what chunk ranges need
  // to be hash checked. If they arn't called then by default it will
  // use an cleared bitfield and check the whole range.
  //
  // These must be called when is_open, !is_checked and !is_checking.
  void                set_bitfield(bool allSet);
  void                set_bitfield(const uint8_t* first, const uint8_t* last);

  static constexpr int update_range_recheck = (1 << 0);
  static constexpr int update_range_clear   = (1 << 1);

  void                update_range(int flags, uint32_t first, uint32_t last);

  // Temporary hack for syncing chunks to disk before hash resume is
  // saved.
  void                sync_chunks();

  uint32_t            peers_complete() const;
  uint32_t            peers_accounted() const;

  uint32_t            peers_currently_unchoked() const;
  uint32_t            peers_currently_interested() const;

  uint32_t            size_pex() const;
  uint32_t            max_size_pex() const;

  bool                accepting_new_peers() const;

  uint32_t            uploads_max() const;
  void                set_uploads_max(uint32_t v);

  uint32_t            uploads_min() const;
  void                set_uploads_min(uint32_t v);

  uint32_t            downloads_max() const;
  void                set_downloads_max(uint32_t v);

  uint32_t            downloads_min() const;
  void                set_downloads_min(uint32_t v);

  void                set_upload_throttle(Throttle* t);
  void                set_download_throttle(Throttle* t);

  // Some temporary functions that are routed to
  // TrackerManager... Clean this up.
  void                send_completed();

  void                manual_request(bool force);
  void                manual_cancel();

  enum ConnectionType {
    CONNECTION_LEECH,
    CONNECTION_SEED,
    CONNECTION_INITIAL_SEED,
    CONNECTION_METADATA,
  };

  ConnectionType      connection_type() const;
  void                set_connection_type(ConnectionType t);

  enum HeuristicType {
  };

  HeuristicType       upload_choke_heuristic() const;
  void                set_upload_choke_heuristic(HeuristicType t);

  HeuristicType       download_choke_heuristic() const;
  void                set_download_choke_heuristic(HeuristicType t);

  // Call this when you want the modifications of the download priorities
  // in the entries to take effect. It is slightly expensive as it rechecks
  // all the peer bitfields to see if we are still interested.
  void                update_priorities();

  void                add_peer(const sockaddr* addr, int port);

  DownloadWrapper*    ptr() { return m_ptr; }
  DownloadMain*       main();

private:
  DownloadWrapper*    m_ptr;
};

} // namespace torrent

#endif
