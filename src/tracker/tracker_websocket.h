#ifndef LIBTORRENT_TRACKER_TRACKER_WEBSOCKET_H
#define LIBTORRENT_TRACKER_TRACKER_WEBSOCKET_H

#include "config.h"

#ifdef USE_WEBTORRENT

#include <optional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "torrent/hash_string.h"
#include "torrent/tracker/tracker_state.h"
#include "tracker/tracker_worker.h"

namespace rtc {
class WebSocket;
} // namespace rtc

namespace torrent {

namespace webtorrent {
class RtcSignaling;
struct RtcStream;
} // namespace webtorrent

struct WebtorrentOffer {
  std::vector<char> offer_id;
  HashString        peer_id{HashString::new_zero()};
  std::string       sdp;
};

struct WebtorrentAnswer {
  std::vector<char> offer_id;
  HashString        peer_id{HashString::new_zero()};
  std::string       sdp;
};

struct WebsocketTrackerResponse {
  HashString info_hash{HashString::new_zero()};

  std::optional<std::chrono::seconds> interval;
  std::optional<std::chrono::seconds> min_interval;
  int complete{-1};
  int incomplete{-1};
  int downloaded{-1};

  std::optional<WebtorrentOffer>  offer;
  std::optional<WebtorrentAnswer> answer;
};

using WebsocketTrackerParseResult = std::variant<WebsocketTrackerResponse, std::string>;

WebsocketTrackerParseResult parse_websocket_tracker_response(std::string_view message);
std::string build_websocket_tracker_announce(const TrackerInfo& info,
                                             const tracker::TrackerParams& params,
                                             tracker::TrackerState::event_enum event,
                                             const std::vector<WebtorrentOffer>& offers = {});
std::string build_websocket_tracker_answer(const TrackerInfo& info, const WebtorrentAnswer& answer);

class TrackerWebsocket : public TrackerWorker {
public:
  TrackerWebsocket(const TrackerInfo& info, int flags = 0);

  tracker_enum type() const override;

  void send_event(tracker::TrackerParams params, tracker::TrackerState::event_enum new_state) override;
  void send_scrape(tracker::TrackerParams params) override;

  void close() override;

private:
  void cleanup() override;
  void close_directly();
  void update_requesting_state();

  void receive_open();
  void receive_message(std::string message);
  void receive_failed(std::string message);
  void receive_closed();
  void receive_rtc_stream(webtorrent::RtcStream stream);
  void send_answer(WebtorrentAnswer answer);

  std::mutex m_websocket_mutex;
  std::shared_ptr<rtc::WebSocket> m_websocket;
  std::shared_ptr<webtorrent::RtcSignaling> m_rtc_signaling;
  tracker::TrackerParams m_params;
};

} // namespace torrent

#endif // USE_WEBTORRENT

#endif // LIBTORRENT_TRACKER_TRACKER_WEBSOCKET_H
