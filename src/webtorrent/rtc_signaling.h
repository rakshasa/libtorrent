#ifndef LIBTORRENT_WEBTORRENT_RTC_SIGNALING_H
#define LIBTORRENT_WEBTORRENT_RTC_SIGNALING_H

#include "config.h"

#ifdef USE_WEBTORRENT

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <rtc/rtc.hpp>

#include "torrent/hash_string.h"
#include "tracker/tracker_websocket.h"

namespace torrent::webtorrent {

struct RtcStream {
  std::shared_ptr<rtc::PeerConnection> peer_connection;
  std::shared_ptr<rtc::DataChannel> data_channel;
};

class RtcSignaling : public std::enable_shared_from_this<RtcSignaling> {
public:
  using offer_callback = std::function<void(std::string, WebtorrentOffer)>;
  using answer_callback = std::function<void(std::string, WebtorrentAnswer)>;
  using stream_callback = std::function<void(RtcStream)>;

  explicit RtcSignaling(HashString local_id, stream_callback stream_cb);
  ~RtcSignaling();

  RtcSignaling(const RtcSignaling&) = delete;
  RtcSignaling& operator=(const RtcSignaling&) = delete;

  void close();

  void generate_offer(offer_callback cb);
  void process_offer(const WebtorrentOffer& offer, answer_callback cb);
  void process_answer(const WebtorrentAnswer& answer);

private:
  using description_callback = std::function<void(std::string, std::string)>;

  struct Connection {
    std::shared_ptr<rtc::PeerConnection> peer_connection;
    std::shared_ptr<rtc::DataChannel> data_channel;
    HashString peer_id{HashString::new_zero()};
    description_callback description_cb;
  };

  std::vector<char> generate_offer_id() const;
  Connection& create_connection(const std::vector<char>& offer_id, description_callback cb);
  void handle_data_channel(const std::vector<char>& offer_id, std::shared_ptr<rtc::DataChannel> dc);

  mutable std::mutex m_mutex;
  HashString m_local_id;
  stream_callback m_stream_cb;
  std::map<std::vector<char>, Connection> m_connections;
};

} // namespace torrent::webtorrent

#endif // USE_WEBTORRENT

#endif // LIBTORRENT_WEBTORRENT_RTC_SIGNALING_H
