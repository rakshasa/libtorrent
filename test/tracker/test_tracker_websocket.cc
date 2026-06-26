#include "config.h"

#include "test/tracker/test_tracker_websocket.h"

#include <variant>

#include <cppunit/extensions/HelperMacros.h>

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(test_tracker_websocket, "tracker");

#ifdef USE_WEBTORRENT

#include <condition_variable>
#include <mutex>
#include <nlohmann/json.hpp>

#include "test/torrent/test_tracker_list.h"
#include "tracker/tracker_websocket.h"
#include "webtorrent/data_channel_stream.h"
#include "webtorrent/rtc_signaling.h"

namespace {

const torrent::WebsocketTrackerResponse&
parse_success(const char* message) {
  static torrent::WebsocketTrackerParseResult result;

  result = torrent::parse_websocket_tracker_response(message);

  CPPUNIT_ASSERT(std::holds_alternative<torrent::WebsocketTrackerResponse>(result));
  return std::get<torrent::WebsocketTrackerResponse>(result);
}

void
assert_parse_failure(const char* message) {
  auto result = torrent::parse_websocket_tracker_response(message);

  CPPUNIT_ASSERT(std::holds_alternative<std::string>(result));
}

} // namespace

void
test_tracker_websocket::test_insert_url() {
  TRACKER_LIST_SETUP();

  tracker_list.insert_url(0, "ws://tracker.example.com/announce");
  CPPUNIT_ASSERT(tracker_list.back().type() == torrent::TRACKER_WEBSOCKET);
  CPPUNIT_ASSERT(!tracker_list.back().is_scrapable());

  tracker_list.insert_url(0, "wss://tracker.example.com/announce");
  CPPUNIT_ASSERT(tracker_list.back().type() == torrent::TRACKER_WEBSOCKET);
  CPPUNIT_ASSERT(!tracker_list.back().is_scrapable());

  tracker_list.clear();
  std::this_thread::sleep_for(100ms);
}

void
test_tracker_websocket::test_parse_announce() {
  auto& parsed = parse_success(
      R"({"complete":1,"incomplete":0,"downloaded":2,"action":"announce","interval":120,"min_interval":60,"info_hash":"xxxxxxxxxxxxxxxxxxxx"})");

  CPPUNIT_ASSERT(parsed.info_hash.str() == "xxxxxxxxxxxxxxxxxxxx");
  CPPUNIT_ASSERT(parsed.interval == 120s);
  CPPUNIT_ASSERT(parsed.min_interval == 60s);
  CPPUNIT_ASSERT(parsed.complete == 1);
  CPPUNIT_ASSERT(parsed.incomplete == 0);
  CPPUNIT_ASSERT(parsed.downloaded == 2);
  CPPUNIT_ASSERT(!parsed.offer);
  CPPUNIT_ASSERT(!parsed.answer);
}

void
test_tracker_websocket::test_parse_offer() {
  auto& parsed = parse_success(
      R"({"action":"announce","offer":{"type":"offer","sdp":"SDP\r\n"},"offer_id":"yyyyyyyyyyyyyyyy","peer_id":"-LT100F-p!SALH(DnYsi","info_hash":"xxxxxxxxxxxxxxxxxxxx"})");

  CPPUNIT_ASSERT(parsed.info_hash.str() == "xxxxxxxxxxxxxxxxxxxx");
  CPPUNIT_ASSERT(parsed.offer);
  CPPUNIT_ASSERT(!parsed.answer);
  CPPUNIT_ASSERT(!parsed.interval);

  CPPUNIT_ASSERT(std::string(parsed.offer->offer_id.begin(), parsed.offer->offer_id.end()) == "yyyyyyyyyyyyyyyy");
  CPPUNIT_ASSERT(parsed.offer->peer_id.str() == "-LT100F-p!SALH(DnYsi");
  CPPUNIT_ASSERT(parsed.offer->sdp == "SDP\r\n");
}

void
test_tracker_websocket::test_parse_answer() {
  auto& parsed = parse_success(
      R"({"action":"announce","answer":{"type":"answer","sdp":"SDP\r\n"},"offer_id":"yyyyyyyyyyyyyyyy","peer_id":"-LT100F-p!SALH(DnYsi","info_hash":"xxxxxxxxxxxxxxxxxxxx"})");

  CPPUNIT_ASSERT(parsed.info_hash.str() == "xxxxxxxxxxxxxxxxxxxx");
  CPPUNIT_ASSERT(parsed.answer);
  CPPUNIT_ASSERT(!parsed.offer);
  CPPUNIT_ASSERT(!parsed.interval);

  CPPUNIT_ASSERT(std::string(parsed.answer->offer_id.begin(), parsed.answer->offer_id.end()) == "yyyyyyyyyyyyyyyy");
  CPPUNIT_ASSERT(parsed.answer->peer_id.str() == "-LT100F-p!SALH(DnYsi");
  CPPUNIT_ASSERT(parsed.answer->sdp == "SDP\r\n");
}

void
test_tracker_websocket::test_parse_invalid() {
  assert_parse_failure(R"({"invalid":foo)");
  assert_parse_failure(R"([ "foo" ])");
  assert_parse_failure(R"({"interval":120})");
  assert_parse_failure(R"({"interval":120,"info_hash":"short"})");
  assert_parse_failure(R"({"interval":"bad","info_hash":"xxxxxxxxxxxxxxxxxxxx"})");
  assert_parse_failure(R"({"offer":{"type":"offer"},"offer_id":"yyyyyyyyyyyyyyyy","peer_id":"-LT100F-p!SALH(DnYsi","info_hash":"xxxxxxxxxxxxxxxxxxxx"})");
  assert_parse_failure(R"({"answer":{"type":"answer"},"offer_id":"yyyyyyyyyyyyyyyy","peer_id":"-LT100F-p!SALH(DnYsi","info_hash":"xxxxxxxxxxxxxxxxxxxx"})");
  assert_parse_failure(R"({"offer":"bad","offer_id":"yyyyyyyyyyyyyyyy","peer_id":"-LT100F-p!SALH(DnYsi","info_hash":"xxxxxxxxxxxxxxxxxxxx"})");
  assert_parse_failure(R"({"answer":["bad"],"offer_id":"yyyyyyyyyyyyyyyy","peer_id":"-LT100F-p!SALH(DnYsi","info_hash":"xxxxxxxxxxxxxxxxxxxx"})");
  assert_parse_failure(R"({"offer":{"type":"offer","sdp":"SDP"},"offer_id":"yyyyyyyyyyyyyyyy","peer_id":"short","info_hash":"xxxxxxxxxxxxxxxxxxxx"})");
}

void
test_tracker_websocket::test_parse_binary_hash() {
  torrent::TrackerInfo info;
  std::string raw_hash;

  for (int i = 0; i < 20; ++i)
    raw_hash.push_back(static_cast<char>(0xf0 - i));

  info.info_hash.assign(raw_hash.data());
  info.local_id.assign("-LT100F-p!SALH(DnYsi");

  auto message = torrent::build_websocket_tracker_announce(info, torrent::tracker::TrackerParams{}, torrent::tracker::TrackerState::EVENT_NONE);
  auto result = torrent::parse_websocket_tracker_response(message);

  CPPUNIT_ASSERT(std::holds_alternative<torrent::WebsocketTrackerResponse>(result));
  CPPUNIT_ASSERT(std::get<torrent::WebsocketTrackerResponse>(result).info_hash == info.info_hash);
}

void
test_tracker_websocket::test_build_announce() {
  torrent::TrackerInfo info;
  info.info_hash.assign("xxxxxxxxxxxxxxxxxxxx");
  info.local_id.assign("-LT100F-p!SALH(DnYsi");
  info.key = 0x1234abcd;

  torrent::tracker::TrackerParams params;
  params.numwant = 11;
  params.uploaded_adjusted = 22;
  params.completed_adjusted = 33;
  params.download_left = 44;

  auto message = torrent::build_websocket_tracker_announce(info, params, torrent::tracker::TrackerState::EVENT_STARTED);
  auto payload = nlohmann::json::parse(message);

  CPPUNIT_ASSERT(payload["action"] == "announce");
  CPPUNIT_ASSERT(payload["info_hash"] == "xxxxxxxxxxxxxxxxxxxx");
  CPPUNIT_ASSERT(payload["peer_id"] == "-LT100F-p!SALH(DnYsi");
  CPPUNIT_ASSERT(payload["key"] == "1234ABCD");
  CPPUNIT_ASSERT(payload["event"] == "started");
  CPPUNIT_ASSERT(payload["numwant"] == 11);
  CPPUNIT_ASSERT(payload["uploaded"] == 22);
  CPPUNIT_ASSERT(payload["downloaded"] == 33);
  CPPUNIT_ASSERT(payload["left"] == 44);
  CPPUNIT_ASSERT(payload["offers"].is_array());
  CPPUNIT_ASSERT(payload["offers"].empty());
}

void
test_tracker_websocket::test_build_answer() {
  torrent::TrackerInfo info;
  info.info_hash.assign("xxxxxxxxxxxxxxxxxxxx");
  info.local_id.assign("-LT100F-local-peer!x");

  torrent::WebtorrentAnswer answer;
  answer.offer_id.assign({'o', 'f', 'f', 'e', 'r'});
  answer.peer_id.assign("-LT100F-remote-peerx");
  answer.sdp = "SDP\r\n";

  auto message = torrent::build_websocket_tracker_answer(info, answer);
  auto payload = nlohmann::json::parse(message);

  CPPUNIT_ASSERT(payload["action"] == "announce");
  CPPUNIT_ASSERT(payload["info_hash"] == "xxxxxxxxxxxxxxxxxxxx");
  CPPUNIT_ASSERT(payload["peer_id"] == "-LT100F-local-peer!x");
  CPPUNIT_ASSERT(payload["to_peer_id"] == "-LT100F-remote-peerx");
  CPPUNIT_ASSERT(payload["offer_id"] == "offer");
  CPPUNIT_ASSERT(payload["answer"]["type"] == "answer");
  CPPUNIT_ASSERT(payload["answer"]["sdp"] == "SDP\r\n");
}

void
test_tracker_websocket::test_rtc_signaling() {
  torrent::HashString local_id_1;
  torrent::HashString local_id_2;

  local_id_1.assign("-LT100F-p!SALH(DnYs1");
  local_id_2.assign("-LT100F-p!SALH(DnYs2");

  std::mutex mutex;
  std::condition_variable cond;
  std::vector<torrent::webtorrent::RtcStream> streams;

  auto on_stream = [&](torrent::webtorrent::RtcStream stream) {
    std::scoped_lock guard(mutex);
    streams.push_back(std::move(stream));
    cond.notify_all();
  };

  auto signaling_1 = std::make_shared<torrent::webtorrent::RtcSignaling>(local_id_1, on_stream);
  auto signaling_2 = std::make_shared<torrent::webtorrent::RtcSignaling>(local_id_2, on_stream);

  std::optional<torrent::WebtorrentOffer> offer;
  std::optional<torrent::WebtorrentAnswer> answer;
  std::string offer_error;
  std::string answer_error;

  signaling_1->generate_offer([&](std::string error, torrent::WebtorrentOffer generated_offer) {
    std::scoped_lock guard(mutex);
    offer_error = std::move(error);
    offer = std::move(generated_offer);
    cond.notify_all();
  });

  {
    std::unique_lock lock(mutex);
    CPPUNIT_ASSERT(cond.wait_for(lock, 15s, [&] { return offer.has_value() || !offer_error.empty(); }));
  }

  CPPUNIT_ASSERT(offer_error.empty());
  CPPUNIT_ASSERT(offer.has_value());

  signaling_2->process_offer(*offer, [&](std::string error, torrent::WebtorrentAnswer generated_answer) {
    std::scoped_lock guard(mutex);
    answer_error = std::move(error);
    answer = std::move(generated_answer);
    cond.notify_all();
  });

  {
    std::unique_lock lock(mutex);
    CPPUNIT_ASSERT(cond.wait_for(lock, 15s, [&] { return answer.has_value() || !answer_error.empty(); }));
  }

  CPPUNIT_ASSERT(answer_error.empty());
  CPPUNIT_ASSERT(answer.has_value());

  signaling_1->process_answer(*answer);

  {
    std::unique_lock lock(mutex);
    CPPUNIT_ASSERT(cond.wait_for(lock, 15s, [&] { return streams.size() >= 2; }));
  }

  torrent::webtorrent::DataChannelStream stream_1(std::move(streams[0]));
  torrent::webtorrent::DataChannelStream stream_2(std::move(streams[1]));

  stream_2.slot_readable([&] {
    std::scoped_lock guard(mutex);
    cond.notify_all();
  });

  const char message[] = "webtorrent-ping";
  CPPUNIT_ASSERT(stream_1.write_stream(message, sizeof(message) - 1) == sizeof(message) - 1);

  {
    std::unique_lock lock(mutex);
    CPPUNIT_ASSERT(cond.wait_for(lock, 5s, [&] { return stream_2.available() >= sizeof(message) - 1; }));
  }

  char read_buffer[32]{};
  CPPUNIT_ASSERT(stream_2.read_stream(read_buffer, 3) == 3);
  CPPUNIT_ASSERT(std::string(read_buffer, 3) == "web");

  CPPUNIT_ASSERT(stream_2.read_stream(read_buffer, sizeof(read_buffer)) == sizeof(message) - 4);
  CPPUNIT_ASSERT(std::string(read_buffer, sizeof(message) - 4) == "torrent-ping");

  signaling_1->close();
  signaling_2->close();
}

#else

void test_tracker_websocket::test_insert_url() {}
void test_tracker_websocket::test_parse_announce() {}
void test_tracker_websocket::test_parse_offer() {}
void test_tracker_websocket::test_parse_answer() {}
void test_tracker_websocket::test_parse_invalid() {}
void test_tracker_websocket::test_parse_binary_hash() {}
void test_tracker_websocket::test_build_announce() {}
void test_tracker_websocket::test_build_answer() {}
void test_tracker_websocket::test_rtc_signaling() {}

#endif // USE_WEBTORRENT
