#include "config.h"

#include "tracker/tracker_http.h"

#include <iomanip>
#include <sstream>
#include <utility>

#include "net/address_list.h"
#include "rak/string_manip.h"
#include "torrent/connection_manager.h"
#include "torrent/exceptions.h"
#include "torrent/net/http_stack.h"
#include "torrent/net/network_config.h"
#include "torrent/net/network_manager.h"
#include "torrent/net/socket_address.h"
#include "torrent/object_stream.h"
#include "torrent/utils/log.h"
#include "torrent/utils/option_strings.h"
#include "torrent/utils/string_manip.h"
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

tracker_enum
TrackerHttp::type() const {
  return TRACKER_HTTP;
}

bool
TrackerHttp::is_busy() const {
  return m_data != nullptr;
}

void
TrackerHttp::send_event(tracker::TrackerState::event_enum new_state) {
  LT_LOG("sending event : state:%s url:%s", option_as_string(OPTION_TRACKER_EVENT, new_state), info().url.c_str());

  close_directly();
  this_thread::scheduler()->erase(&m_delay_scrape);

  lock_and_set_latest_event(new_state);

  // TODO: Move to network config and add simple retry other AF logic.

  // TODO: We need to handle retry logic here, not in http stack, as we need to change the bind
  // address? Or is this handled by the http stack?
  //
  // TODO: We need to include ipv4/ipv6 param in tracker requests if we bind to the other, so it
  // needs to be handled here.

  bool is_block_ipv4 = config::network_config()->is_block_ipv4();
  bool is_block_ipv6 = config::network_config()->is_block_ipv6();
  bool is_prefer_ipv6 = config::network_config()->is_prefer_ipv6();
  int family{};

  // If both IPv4 and IPv6 are blocked, we cannot send the request.
  //
  // TODO: Properly handle this case without throwing an error.

  if (is_block_ipv4 && is_block_ipv6) {
    throw torrent::internal_error("Cannot send tracker event, both IPv4 and IPv6 are blocked.");
  } else if (is_block_ipv4) {
    family = AF_INET6;
  } else if (is_block_ipv6) {
    family = AF_INET;
  } else if (is_prefer_ipv6) {
    family = AF_INET6;
  } else {
    family = AF_INET;
  }

  auto params = m_slot_parameters();
  auto request_url = request_announce_url(new_state, params, family);

  m_data = std::make_unique<std::stringstream>();

  m_get.try_wait_for_close();
  m_get.reset(request_url, m_data);

  // TODO: Should bind address here...

  if (is_block_ipv4 && is_block_ipv6) {
    throw torrent::internal_error("Cannot send tracker event, both IPv4 and IPv6 are blocked.");
  } else if (is_block_ipv4) {
    m_get.use_ipv6();
  } else if (is_block_ipv6) {
    m_get.use_ipv4();
  } else if (is_prefer_ipv6) {
    m_get.prefer_ipv6();
  } else {
    m_get.prefer_ipv4();
  }

  LT_LOG_DUMP(request_url.c_str(), request_url.size(),
              "sending event : state:%s up_adj:%" PRIu64 " completed_adj:%" PRIu64 " left_adj:%" PRIu64,
              option_as_string(OPTION_TRACKER_EVENT, new_state),
              params.uploaded_adjusted, params.completed_adjusted, params.download_left);

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

void
TrackerHttp::request_prefix(std::stringstream* stream, const std::string& url) {
  char hash[61];

  *rak::copy_escape_html(info().info_hash.begin(),
                         info().info_hash.end(), hash) = '\0';
  *stream << url
          << (m_drop_deliminator ? '&' : '?')
          << "info_hash=" << hash;
}

std::string
TrackerHttp::request_announce_url(tracker::TrackerState::event_enum state, TrackerParameters params, int family) {
  std::stringstream s;
  s.imbue(std::locale::classic());

  auto tracker_id = this->tracker_id();

  request_prefix(&s, info().url);

  char local_id[61];
  *rak::copy_escape_html(info().local_id.begin(), info().local_id.end(), local_id) = '\0';

  s << "&peer_id=" << local_id
    << "&compact=1";

  if (info().key)
    s << "&key=" << std::hex << std::setw(8) << std::setfill('0') << info().key << std::dec;

  if (!tracker_id.empty())
    s << "&trackerid=" << rak::copy_escape_html(tracker_id);

  auto local_inet_address  = config::network_config()->local_inet_address_or_null();
  auto local_inet6_address = config::network_config()->local_inet6_address_or_null();

  if (local_inet_address != nullptr) {
    if (family == AF_INET6)
      s << "&ip=" << sa_addr_str(local_inet_address.get());

    s << "&ipv4=" << sa_addr_str(local_inet_address.get());
  }

  if (local_inet6_address != nullptr) {
    if (family == AF_INET)
      s << "&ip=" << sa_addr_str(local_inet6_address.get());

    s << "&ipv6=" << sa_addr_str(local_inet6_address.get());
  }

  if (params.numwant >= 0 && state != tracker::TrackerState::EVENT_STOPPED)
    s << "&numwant=" << params.numwant;

  s << "&port=" << runtime::network_manager()->listen_port_or_throw()
    << "&uploaded=" << params.uploaded_adjusted
    << "&downloaded=" << params.completed_adjusted
    << "&left=" << params.download_left;

  switch (state) {
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

  return s.str();
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

  m_get.try_wait_for_close();
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
  m_get.close_and_cancel_callbacks(this_thread::thread());
  m_data.reset();
}

void
TrackerHttp::receive_done() {
  if (m_data == nullptr)
    throw internal_error("TrackerHttp::receive_done() called on an invalid object");

  LT_LOG("received reply", 0);

  if (lt_log_is_valid(LOG_TRACKER_DUMP)) {
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
    auto dump = utils::sanitize_string_with_tags(m_data->str());

    if (dump.empty())
      return receive_failed("Could not parse bencoded data, empty reply");

    return receive_failed("Could not parse bencoded data: " + dump.substr(0,99));
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

  if (lt_log_is_valid(LOG_TRACKER_DUMP)) {
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

  if (!object.has_key("peers") && !object.has_key("peers6")) {
    if (state().latest_event() != tracker::TrackerState::EVENT_STOPPED)
      return receive_failed("No peers returned");

    close_directly();
    m_slot_success(AddressList());
  }

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
