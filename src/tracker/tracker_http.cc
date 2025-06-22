#include "config.h"

#define __STDC_FORMAT_MACROS

#include "tracker/tracker_http.h"

#include <iomanip>
#include <sstream>
#include <utility>
#include <rak/string_manip.h>

#include "net/address_list.h"
#include "torrent/connection_manager.h"
#include "torrent/exceptions.h"
#include "torrent/net/http_stack.h"
#include "torrent/net/socket_address.h"
#include "torrent/net/utils.h"
#include "torrent/object_stream.h"
#include "torrent/utils/log.h"
#include "torrent/utils/option_strings.h"
#include "torrent/utils/uri_parser.h"

#include "manager.h"

#define LT_LOG(log_fmt, ...)                                            \
  lt_log_print_hash(LOG_TRACKER_REQUESTS, info().info_hash, "tracker_http", "%p : " log_fmt, static_cast<TrackerWorker*>(this), __VA_ARGS__);

#define LT_LOG_DUMP(log_dump_data, log_dump_size, log_fmt, ...)         \
  lt_log_print_hash_dump(LOG_TRACKER_DUMP, log_dump_data, log_dump_size, info().info_hash, \
                         "tracker_http", "%p : " log_fmt, static_cast<TrackerWorker*>(this), __VA_ARGS__);

namespace torrent {

TrackerHttp::TrackerHttp(const TrackerInfo& info, int flags)
  : TrackerWorker(info, utils::uri_can_scrape(info.url) ? (flags | tracker::TrackerState::flag_scrapable) : flags),
    m_drop_deliminator(utils::uri_has_query(info.url)) {

  m_get.reset(info.url, nullptr);

  m_get.add_done_slot([this] { receive_done(); });
  m_get.add_failed_slot([this](const auto& str) { receive_signal_failed(str); });

  m_delay_scrape.slot() = [this] { delayed_send_scrape(); };
}

TrackerHttp::~TrackerHttp() {
  close_directly();
  this_thread::scheduler()->erase(&m_delay_scrape);
}

bool
TrackerHttp::is_busy() const {
  return m_data != nullptr;
}

void
TrackerHttp::request_prefix(std::stringstream* stream, const std::string& url) {
  char hash[61];

  *rak::copy_escape_html(info().info_hash.begin(),
                         info().info_hash.end(), hash) = '\0';
  *stream << url
          << (m_drop_deliminator ? '&' : '?')
          << "info_hash=" << hash;
}

void
TrackerHttp::send_event(tracker::TrackerState::event_enum new_state) {
  LT_LOG("sending event : state:%s url:%s", option_as_string(OPTION_TRACKER_EVENT, new_state), info().url.c_str());

  close_directly();
  this_thread::scheduler()->erase(&m_delay_scrape);

  lock_and_set_latest_event(new_state);

  std::stringstream s;
  s.imbue(std::locale::classic());

  char localId[61];

  auto tracker_id = this->tracker_id();

  request_prefix(&s, info().url);

  *rak::copy_escape_html(info().local_id.begin(), info().local_id.end(), localId) = '\0';

  s << "&peer_id=" << localId
    << "&compact=1";

  if (info().key)
    s << "&key=" << std::hex << std::setw(8) << std::setfill('0') << info().key << std::dec;

  if (!tracker_id.empty())
    s << "&trackerid=" << rak::copy_escape_html(tracker_id);

  const rak::socket_address* localAddress = rak::socket_address::cast_from(manager->connection_manager()->local_address());

  if (localAddress->is_address_any()) {
    if (manager->connection_manager()->is_prefer_ipv6()) {
      auto ipv6_address = detect_local_sin6_addr();

      if (ipv6_address != nullptr) {
        s << "&ip=" << sin6_addr_str(ipv6_address.get());
      }
    }
  } else {
    s << "&ip=" << localAddress->address_str();
  }

  auto parameters = m_slot_parameters();

  if (parameters.numwant >= 0 && new_state != tracker::TrackerState::EVENT_STOPPED)
    s << "&numwant=" << parameters.numwant;

  if (manager->connection_manager()->listen_port())
    s << "&port=" << manager->connection_manager()->listen_port();

  s << "&uploaded=" << parameters.uploaded_adjusted
    << "&downloaded=" << parameters.completed_adjusted
    << "&left=" << parameters.download_left;

  switch(new_state) {
  case tracker::TrackerState::EVENT_STARTED:
    s << "&event=started";
    break;
  case tracker::TrackerState::EVENT_STOPPED:
    s << "&event=stopped";
    break;
  case tracker::TrackerState::EVENT_COMPLETED:
    s << "&event=completed";
    break;
  default:
    break;
  }

  m_data = std::make_unique<std::stringstream>();

  std::string request_url = s.str();
  m_get.reset(request_url, m_data);

  LT_LOG_DUMP(request_url.c_str(), request_url.size(),
              "sending event : state:%s up_adj:%" PRIu64 " completed_adj:%" PRIu64 " left_adj:%" PRIu64,
              option_as_string(OPTION_TRACKER_EVENT, new_state),
              parameters.uploaded_adjusted, parameters.completed_adjusted, parameters.download_left);

  torrent::net_thread::http_stack()->start_get(m_get);
}

void
TrackerHttp::send_scrape() {
  if (m_requested_scrape) {
    LT_LOG("scrape already requested : url:%s", info().url.c_str());
    return;
  }

  m_requested_scrape = true;

  if (is_busy()) {
    LT_LOG("scrape requested, but tracker is busy : url:%s", info().url.c_str());
    return;
  }

  LT_LOG("scrape requested : url:%s", info().url.c_str());
  this_thread::scheduler()->wait_for_ceil_seconds(&m_delay_scrape, 10s);
}

// We delay scrape for 10 seconds after any tracker activity to ensure all callbacks are process
// before starting.
void
TrackerHttp::delayed_send_scrape() {
  if (is_busy())
    throw internal_error("TrackerHttp::delayed_send_scrape() called while busy");

  LT_LOG("sending delayed scrape request : url:%s", info().url.c_str());

  close_directly();

  lock_and_set_latest_event(tracker::TrackerState::EVENT_SCRAPE);

  std::stringstream s;
  s.imbue(std::locale::classic());

  request_prefix(&s, utils::uri_generate_scrape_url(info().url));

  std::string request_url = s.str();

  m_data = std::make_unique<std::stringstream>();
  m_get.reset(request_url, m_data);

  LT_LOG_DUMP(request_url.c_str(), request_url.size(), "tracker scrape", 0);
  torrent::net_thread::http_stack()->start_get(m_get);
}

void
TrackerHttp::close() {
  LT_LOG("closing event : state:%s url:%s",
         option_as_string(OPTION_TRACKER_EVENT, state().latest_event()), info().url.c_str());

  this_thread::scheduler()->erase(&m_delay_scrape);
  m_requested_scrape = false;

  close_directly();
}

tracker_enum
TrackerHttp::type() const {
  return TRACKER_HTTP;
}

void
TrackerHttp::close_directly() {
  if (m_data == nullptr) {
    LT_LOG("closing directly (already closed) : state:%s url:%s",
           option_as_string(OPTION_TRACKER_EVENT, state().latest_event()), info().url.c_str());

    m_slot_close();
    return;
  }

  LT_LOG("closing directly : state:%s url:%s",
         option_as_string(OPTION_TRACKER_EVENT, state().latest_event()), info().url.c_str());

  m_slot_close();

  m_get.close();
  m_get.cancel_slot_callbacks(this_thread::thread());

  m_data.reset();
}

void
TrackerHttp::receive_done() {
  if (m_data == nullptr)
    throw internal_error("TrackerHttp::receive_done() called on an invalid object");

  LT_LOG("received reply", 0);

  if (lt_log_is_valid(LOG_TRACKER_DEBUG)) {
    std::string dump = m_data->str();
    LT_LOG_DUMP(dump.c_str(), dump.size(), "tracker reply", 0);
  }

  Object b;
  *m_data >> b;

  // Temporarily reset the interval
  //
  // TODO: This might be causing an issue with too frequent tracker requests.
  lock_and_clear_intervals();

  if (m_data->fail()) {
    std::string dump = m_data->str();
    return receive_failed("Could not parse bencoded data: " + rak::sanitize(rak::striptags(dump)).substr(0,99));
  }

  if (!b.is_map())
    return receive_failed("Root not a bencoded map");

  if (b.has_key("failure reason")) {
    if (state().latest_event() != tracker::TrackerState::EVENT_SCRAPE)
      process_failure(b);

    return receive_failed("Failure reason \"" +
                         (b.get_key("failure reason").is_string() ?
                          b.get_key_string("failure reason") :
                          std::string("failure reason not a string"))
                         + "\"");
  }

  // If no failures, set intervals to defaults prior to processing

  if (state().latest_event() == tracker::TrackerState::EVENT_SCRAPE) {
    m_requested_scrape = false;
    process_scrape(b);
    return;
  }

  process_success(b);

  if (m_requested_scrape && !is_busy())
    this_thread::scheduler()->wait_for_ceil_seconds(&m_delay_scrape, 10s);
}

void
TrackerHttp::receive_signal_failed(const std::string& msg) {
  lock_and_clear_intervals();

  return receive_failed(msg);
}

void
TrackerHttp::receive_failed(const std::string& msg) {
  if (m_data == nullptr)
    throw internal_error("TrackerHttp::receive_failed() called on an invalid object");

  LT_LOG("received failure : msg:%s", msg.c_str());

  if (lt_log_is_valid(LOG_TRACKER_DEBUG)) {
    std::string dump = m_data->str();
    LT_LOG_DUMP(dump.c_str(), dump.size(), "received failure", 0);
  }

  close_directly();

  if (state().latest_event() == tracker::TrackerState::EVENT_SCRAPE) {
    m_requested_scrape = false;
    m_slot_scrape_failure(msg);
    return;
  }

  m_slot_failure(msg);

  if (m_requested_scrape && !is_busy())
    this_thread::scheduler()->wait_for_ceil_seconds(&m_delay_scrape, 10s);
}

void
TrackerHttp::process_failure(const Object& object) {
  auto guard = lock_guard();

  if (object.has_key_string("tracker id"))
    update_tracker_id(object.get_key_string("tracker id"));

  if (object.has_key_value("interval"))
    state().set_normal_interval(object.get_key_value("interval"));

  if (object.has_key_value("min interval"))
    state().set_min_interval(object.get_key_value("min interval"));

  if (object.has_key_value("complete") && object.has_key_value("incomplete")) {
    state().m_scrape_complete = std::max<int64_t>(object.get_key_value("complete"), 0);
    state().m_scrape_incomplete = std::max<int64_t>(object.get_key_value("incomplete"), 0);
    state().m_scrape_time_last = this_thread::cached_seconds().count();
  }

  if (object.has_key_value("downloaded"))
    state().m_scrape_downloaded = std::max<int64_t>(object.get_key_value("downloaded"), 0);
}

void
TrackerHttp::process_success(const Object& object) {
  {
    auto guard = lock_guard();

    if (object.has_key_string("tracker id"))
      update_tracker_id(object.get_key_string("tracker id"));

    if (object.has_key_value("interval"))
      state().set_normal_interval(object.get_key_value("interval"));
    else
      state().set_normal_interval(tracker::TrackerState::default_normal_interval);

    if (object.has_key_value("min interval"))
      state().set_min_interval(object.get_key_value("min interval"));
    else
      state().set_min_interval(tracker::TrackerState::default_min_interval);

    if (object.has_key_value("complete") && object.has_key_value("incomplete")) {
      state().m_scrape_complete = std::max<int64_t>(object.get_key_value("complete"), 0);
      state().m_scrape_incomplete = std::max<int64_t>(object.get_key_value("incomplete"), 0);
      state().m_scrape_time_last = this_thread::cached_seconds().count();
    }

    if (object.has_key_value("downloaded"))
      state().m_scrape_downloaded = std::max<int64_t>(object.get_key_value("downloaded"), 0);
  }

  if (!object.has_key("peers") && !object.has_key("peers6"))
    return receive_failed("No peers returned");

  AddressList l;

  if (object.has_key("peers")) {
    try {
      // Due to some trackers sending the wrong type when no peers are
      // available, don't bork on it.
      if (object.get_key("peers").is_string())
        l.parse_address_compact(object.get_key_string("peers"));

      else if (object.get_key("peers").is_list())
        l.parse_address_normal(object.get_key_list("peers"));

    } catch (const bencode_error& e) {
      return receive_failed(e.what());
    }
  }

  if (object.has_key_string("peers6"))
    l.parse_address_compact_ipv6(object.get_key_string("peers6"));

  close_directly();
  m_slot_success(std::move(l));
}

void
TrackerHttp::process_scrape(const Object& object) {
  if (!object.has_key_map("files"))
    return receive_failed("Tracker scrape does not have files entry.");

  // Add better validation here...
  const Object& files = object.get_key("files");

  if (!files.has_key_map(info().info_hash.str()))
    return receive_failed("Tracker scrape replay did not contain infohash.");

  const Object& stats = files.get_key(info().info_hash.str());

  {
    auto guard = lock_guard();

    if (stats.has_key_value("complete"))
      state().m_scrape_complete = std::max<int64_t>(stats.get_key_value("complete"), 0);

    if (stats.has_key_value("incomplete"))
      state().m_scrape_incomplete = std::max<int64_t>(stats.get_key_value("incomplete"), 0);

    if (stats.has_key_value("downloaded"))
      state().m_scrape_downloaded = std::max<int64_t>(stats.get_key_value("downloaded"), 0);

    LT_LOG("tracker scrape for %zu torrents : complete:%u incomplete:%u downloaded:%u",
           files.as_map().size(), state().m_scrape_complete, state().m_scrape_incomplete, state().m_scrape_downloaded);
  }

  close_directly();
  m_slot_scrape_success();
}

void
TrackerHttp::update_tracker_id(const std::string& id) {
  if (id.empty())
    return;

  if (m_current_tracker_id == id)
    return;

  set_tracker_id(id);
}

} // namespace torrent
