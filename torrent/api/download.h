#ifndef LIBTORRENT_DOWNLOAD_H
#define LIBTORRENT_DOWNLOAD_H

#include <inttypes.h>

namespace torrent {

class Download {
public:
  Download();
  Download(void* d);

  bool is_open();
  bool is_active();
  bool is_tracker_busy();

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
  
  uint32_t get_peers_min();
  uint32_t get_peers_max();
  uint32_t get_peers_connected();
  uint32_t get_peers_not_connected();

  uint64_t get_tracker_timeout();

  
