#ifndef LIBTORRENT_DOWNLOAD_H
#define LIBTORRENT_DOWNLOAD_H

#include <torrent/common.h>

namespace torrent {

class Download {
public:
  Download();
  Download(void* d);

  bool is_open();
  bool is_active();
  bool is_tracker_busy();

  std::string get_name();
  std::string get_hash();

  uint64_t get_bytes_up();
  uint64_t get_bytes_down();
  uint64_t get_bytes_done();
  uint64_t get_bytes_total();

  uint32_t get_chunks_size();
  uint32_t get_chunks_done();
  uint32_t get_chunks_total();

  // Bytes per second.
  uint32_t get_rate_up();
  uint32_t get_rate_down();
  
  const char* get_bitfield_data();
  uint32_t    get_bitfield_size();

  uint32_t get_peers_min();
  uint32_t get_peers_max();
  uint32_t get_peers_connected();
  uint32_t get_peers_not_connected();

  uint32_t get_uploads_max();
  
  uint64_t get_tracker_timeout();

  void     set_peers_min(uint32_t v);
  void     set_peers_max(uint32_t v);

  void     set_uploads_max(uint32_t v);

  void     set_tracker_timeout(uint64_t v);

  

private:
  void* m_ptr;
};

}

#endif

