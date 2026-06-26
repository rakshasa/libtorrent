#include "config.h"

#ifdef USE_WEBTORRENT

#include "tracker/tracker_websocket.h"

#include <cstdio>

#include <rtc/rtc.hpp>
#include <nlohmann/json.hpp>

#include "net/address_list.h"
#include "torrent/exceptions.h"
#include "torrent/runtime/runtime.h"
#include "tracker/thread_tracker.h"
#include "thread_main.h"
#include "torrent/utils/option_strings.h"
#include "webtorrent/rtc_signaling.h"

namespace torrent {

namespace {

std::string
latin1_to_utf8(std::string_view input) {
  std::string result;
  result.reserve(input.size());

  for (unsigned char c : input) {
    if (c < 0x80) {
      result.push_back(static_cast<char>(c));
    } else {
      result.push_back(static_cast<char>(0xc0 | (c >> 6)));
      result.push_back(static_cast<char>(0x80 | (c & 0x3f)));
    }
  }

  return result;
}

std::optional<std::string>
utf8_to_latin1(std::string_view input) {
  std::string result;
  result.reserve(input.size());

  for (size_t i = 0; i < input.size(); ++i) {
    const auto c = static_cast<unsigned char>(input[i]);

    if (c < 0x80) {
      result.push_back(static_cast<char>(c));
    } else if ((c & 0xe0) == 0xc0 && i + 1 < input.size()) {
      const auto next = static_cast<unsigned char>(input[++i]);

      if ((next & 0xc0) != 0x80)
        return std::nullopt;

      const auto decoded = static_cast<unsigned int>(((c & 0x1f) << 6) | (next & 0x3f));

      if (decoded < 0x80 || decoded > 0xff)
        return std::nullopt;

      result.push_back(static_cast<char>(decoded));
    } else {
      return std::nullopt;
    }
  }

  return result;
}

bool
assign_hash_string(HashString* hash, const std::string& value, const char* field, std::string* error) {
  if (value.size() != HashString::size_data) {
    *error = std::string("invalid ") + field + " size " + std::to_string(value.size());
    return false;
  }

  hash->assign(value.data());
  return true;
}

std::optional<std::string>
extract_string(const nlohmann::json& object, const char* key) {
  auto itr = object.find(key);

  if (itr == object.end())
    return std::nullopt;

  if (!itr->is_string())
    return std::nullopt;

  return itr->get<std::string>();
}

bool
parse_signaling_common(const nlohmann::json& payload, HashString* peer_id, std::vector<char>* offer_id, std::string* error) {
  auto peer_id_str = extract_string(payload, "peer_id");

  if (!peer_id_str) {
    *error = "missing peer_id";
    return false;
  }

  auto raw_peer_id = utf8_to_latin1(*peer_id_str);

  if (!raw_peer_id) {
    *error = "invalid peer_id encoding";
    return false;
  }

  if (!assign_hash_string(peer_id, *raw_peer_id, "peer_id", error))
    return false;

  auto offer_id_str = extract_string(payload, "offer_id");

  if (!offer_id_str) {
    *error = "missing offer_id";
    return false;
  }

  auto raw_offer_id = utf8_to_latin1(*offer_id_str);

  if (!raw_offer_id) {
    *error = "invalid offer_id encoding";
    return false;
  }

  offer_id->assign(raw_offer_id->begin(), raw_offer_id->end());
  return true;
}

const char*
websocket_event_name(tracker::TrackerState::event_enum event) {
  switch (event) {
  case tracker::TrackerState::EVENT_COMPLETED:
    return "completed";
  case tracker::TrackerState::EVENT_STARTED:
    return "started";
  case tracker::TrackerState::EVENT_STOPPED:
    return "stopped";
  case tracker::TrackerState::EVENT_NONE:
  case tracker::TrackerState::EVENT_SCRAPE:
    return nullptr;
  }

  return nullptr;
}

} // namespace

WebsocketTrackerParseResult
parse_websocket_tracker_response(std::string_view message) {
  try {
    auto payload = nlohmann::json::parse(message);

    if (!payload.is_object())
      return std::string("websocket tracker message is not an object");

    auto info_hash_str = extract_string(payload, "info_hash");

    if (!info_hash_str)
      return std::string("missing info_hash");

    auto raw_info_hash = utf8_to_latin1(*info_hash_str);

    if (!raw_info_hash)
      return std::string("invalid info_hash encoding");

    WebsocketTrackerResponse response;
    std::string error;

    if (!assign_hash_string(&response.info_hash, *raw_info_hash, "info_hash", &error))
      return error;

    if (auto reason = extract_string(payload, "failure reason"))
      return *reason;

    if (auto itr = payload.find("interval"); itr != payload.end()) {
      if (!itr->is_number_integer())
        return std::string("invalid interval");

      response.interval = std::chrono::seconds(itr->get<int64_t>());
    }

    if (auto itr = payload.find("min_interval"); itr != payload.end()) {
      if (!itr->is_number_integer())
        return std::string("invalid min_interval");

      response.min_interval = std::chrono::seconds(itr->get<int64_t>());
    }

    if (auto itr = payload.find("complete"); itr != payload.end()) {
      if (!itr->is_number_integer())
        return std::string("invalid complete");

      response.complete = itr->get<int>();
    }

    if (auto itr = payload.find("incomplete"); itr != payload.end()) {
      if (!itr->is_number_integer())
        return std::string("invalid incomplete");

      response.incomplete = itr->get<int>();
    }

    if (auto itr = payload.find("downloaded"); itr != payload.end()) {
      if (!itr->is_number_integer())
        return std::string("invalid downloaded");

      response.downloaded = itr->get<int>();
    }

    if (auto itr = payload.find("offer"); itr != payload.end()) {
      if (!itr->is_object())
        return std::string("offer is not an object");

      auto sdp = extract_string(*itr, "sdp");

      if (!sdp)
        return std::string("offer missing sdp");

      WebtorrentOffer offer;
      offer.sdp = std::move(*sdp);

      if (!parse_signaling_common(payload, &offer.peer_id, &offer.offer_id, &error))
        return error;

      response.offer = std::move(offer);
    }

    if (auto itr = payload.find("answer"); itr != payload.end()) {
      if (!itr->is_object())
        return std::string("answer is not an object");

      auto sdp = extract_string(*itr, "sdp");

      if (!sdp)
        return std::string("answer missing sdp");

      WebtorrentAnswer answer;
      answer.sdp = std::move(*sdp);

      if (!parse_signaling_common(payload, &answer.peer_id, &answer.offer_id, &error))
        return error;

      response.answer = std::move(answer);
    }

    return response;

  } catch (const std::exception& e) {
    return std::string(e.what());
  }
}

TrackerWebsocket::TrackerWebsocket(const TrackerInfo& info, int flags) :
  TrackerWorker(info, flags) {
}

std::string
build_websocket_tracker_announce(const TrackerInfo& info,
                                 const tracker::TrackerParams& params,
                                 tracker::TrackerState::event_enum event,
                                 const std::vector<WebtorrentOffer>& offers) {
  nlohmann::json payload;

  payload["action"] = "announce";
  payload["info_hash"] = latin1_to_utf8(info.info_hash.str());
  payload["peer_id"] = latin1_to_utf8(info.local_id.str());
  payload["uploaded"] = params.uploaded_adjusted;
  payload["downloaded"] = params.completed_adjusted;
  payload["left"] = params.download_left;
  payload["numwant"] = params.numwant;

  char key[9];
  std::snprintf(key, sizeof(key), "%08X", info.key);
  payload["key"] = key;

  if (auto event_name = websocket_event_name(event))
    payload["event"] = event_name;

  auto offer_array = nlohmann::json::array();

  for (const auto& offer : offers) {
    nlohmann::json offer_payload;
    offer_payload["offer_id"] = latin1_to_utf8(std::string_view(offer.offer_id.data(), offer.offer_id.size()));
    offer_payload["offer"] = {
      {"type", "offer"},
      {"sdp", offer.sdp},
    };

    offer_array.push_back(std::move(offer_payload));
  }

  payload["offers"] = std::move(offer_array);

  return payload.dump();
}

std::string
build_websocket_tracker_answer(const TrackerInfo& info, const WebtorrentAnswer& answer) {
  nlohmann::json payload;

  payload["action"] = "announce";
  payload["info_hash"] = latin1_to_utf8(info.info_hash.str());
  payload["peer_id"] = latin1_to_utf8(info.local_id.str());
  payload["to_peer_id"] = latin1_to_utf8(answer.peer_id.str());
  payload["offer_id"] = latin1_to_utf8(std::string_view(answer.offer_id.data(), answer.offer_id.size()));
  payload["answer"] = {
    {"type", "answer"},
    {"sdp", answer.sdp},
  };

  return payload.dump();
}

tracker_enum
TrackerWebsocket::type() const {
  return TRACKER_WEBSOCKET;
}

void
TrackerWebsocket::send_event(tracker::TrackerParams params, tracker::TrackerState::event_enum new_state) {
  if (!runtime::webtorrent_enabled()) {
    receive_failed("WebTorrent support is disabled.");
    return;
  }

  close_directly();

  lock_and_set_latest_event(new_state);
  m_params = params;

  if (!m_rtc_signaling)
    m_rtc_signaling = std::make_shared<webtorrent::RtcSignaling>(info().local_id, [this](webtorrent::RtcStream stream) {
        receive_rtc_stream(std::move(stream));
      });

  auto websocket = std::make_shared<rtc::WebSocket>();

  websocket->onOpen([this] {
      ThreadTracker::thread_base()->callback(callback_id(), [this] { receive_open(); });
    });
  websocket->onMessage([this](rtc::message_variant data) {
      if (auto message = std::get_if<std::string>(&data)) {
        ThreadTracker::thread_base()->callback(callback_id(), [this, message = *message] { receive_message(message); });
      } else {
        ThreadTracker::thread_base()->callback(callback_id(), [this] { receive_failed("websocket tracker sent a binary message"); });
      }
    });
  websocket->onError([this](std::string error) {
      ThreadTracker::thread_base()->callback(callback_id(), [this, error = std::move(error)] { receive_failed(error); });
    });
  websocket->onClosed([this] {
      ThreadTracker::thread_base()->callback(callback_id(), [this] { receive_closed(); });
    });

  {
    std::scoped_lock guard(m_websocket_mutex);
    m_websocket = websocket;
  }

  update_requesting_state();

  try {
    websocket->open(info().url);
  } catch (const std::exception& e) {
    receive_failed(e.what());
  }
}

void
TrackerWebsocket::send_scrape([[maybe_unused]] tracker::TrackerParams params) {
  throw internal_error("Tracker type WebSocket does not support scrape.");
}

void
TrackerWebsocket::close() {
  close_directly();
  update_requesting_state();
}

void
TrackerWebsocket::cleanup() {
  close_directly();

  if (m_rtc_signaling)
    m_rtc_signaling->close();

  auto guard = lock_guard();

  state().m_flags |= tracker::TrackerState::flag_deleted;
  state().m_flags &= ~tracker::TrackerState::flag_requesting;
  state().m_flags &= ~tracker::TrackerState::flag_starting_request;
}

void
TrackerWebsocket::close_directly() {
  std::shared_ptr<rtc::WebSocket> websocket;

  {
    std::scoped_lock guard(m_websocket_mutex);
    websocket = std::move(m_websocket);
    m_websocket.reset();
  }

  if (!websocket)
    return;

  websocket->onOpen(nullptr);
  websocket->onMessage(nullptr);
  websocket->onError(nullptr);
  websocket->onClosed(nullptr);
  websocket->close();
}

void
TrackerWebsocket::update_requesting_state() {
  auto guard = lock_guard();

  state().m_flags &= ~tracker::TrackerState::flag_starting_request;

  std::scoped_lock websocket_guard(m_websocket_mutex);

  if (m_websocket && !m_websocket->isClosed()) {
    state().m_flags |= tracker::TrackerState::flag_requesting;

    if (state().latest_event() == tracker::TrackerState::EVENT_STOPPED)
      state().m_flags |= tracker::TrackerState::flag_disownable;
    else
      state().m_flags &= ~tracker::TrackerState::flag_disownable;
  } else {
    state().m_flags &= ~tracker::TrackerState::flag_requesting;
    state().m_flags &= ~tracker::TrackerState::flag_disownable;
  }
}

void
TrackerWebsocket::receive_open() {
  if (!m_rtc_signaling) {
    receive_failed("websocket tracker signaling was not initialized");
    return;
  }

  m_rtc_signaling->generate_offer([this](std::string error, WebtorrentOffer offer) {
      if (!error.empty()) {
        ThreadTracker::thread_base()->callback(callback_id(), [this, error = std::move(error)] { receive_failed(error); });
        return;
      }

      std::shared_ptr<rtc::WebSocket> websocket;

      {
        std::scoped_lock guard(m_websocket_mutex);
        websocket = m_websocket;
      }

      if (!websocket)
        return;

      websocket->send(build_websocket_tracker_announce(info(), m_params, lock_and_latest_event(), {std::move(offer)}));
    });
}

void
TrackerWebsocket::receive_message(std::string message) {
  auto result = parse_websocket_tracker_response(message);

  if (std::holds_alternative<std::string>(result)) {
    receive_failed(std::get<std::string>(result));
    return;
  }

  auto response = std::get<WebsocketTrackerResponse>(std::move(result));

  if (response.info_hash != info().info_hash) {
    receive_failed("websocket tracker response had an unknown info_hash");
    return;
  }

  if (response.offer && m_rtc_signaling) {
    m_rtc_signaling->process_offer(*response.offer, [this](std::string error, WebtorrentAnswer answer) {
        if (!error.empty()) {
          ThreadTracker::thread_base()->callback(callback_id(), [this, error = std::move(error)] { receive_failed(error); });
          return;
        }

        send_answer(std::move(answer));
      });
  }

  if (response.answer && m_rtc_signaling) {
    m_rtc_signaling->process_answer(*response.answer);
  }

  const bool is_announce_response =
    response.interval ||
    response.min_interval ||
    response.complete >= 0 ||
    response.incomplete >= 0 ||
    response.downloaded >= 0;

  if (!is_announce_response)
    return;

  {
    auto guard = lock_guard();

    if (response.interval)
      state().set_normal_interval(*response.interval);

    if (response.min_interval)
      state().set_min_interval(*response.min_interval);

    state().add_success_request(this_thread::cached_seconds());
  }

  m_slot_success(AddressList{});
}

void
TrackerWebsocket::receive_failed(std::string message) {
  close_directly();

  {
    auto guard = lock_guard();
    state().add_failed_request(this_thread::cached_seconds());
  }

  update_requesting_state();
  m_slot_failure(std::move(message));
}

void
TrackerWebsocket::receive_closed() {
  {
    std::scoped_lock guard(m_websocket_mutex);
    m_websocket.reset();
  }

  update_requesting_state();
}

void
TrackerWebsocket::receive_rtc_stream(webtorrent::RtcStream stream) {
  if (m_slot_webtorrent_stream) {
    m_slot_webtorrent_stream(std::move(stream));
  }
}

void
TrackerWebsocket::send_answer(WebtorrentAnswer answer) {
  std::shared_ptr<rtc::WebSocket> websocket;

  {
    std::scoped_lock guard(m_websocket_mutex);
    websocket = m_websocket;
  }

  if (websocket) {
    websocket->send(build_websocket_tracker_answer(info(), answer));
  }
}

} // namespace torrent

#endif // USE_WEBTORRENT
