#include "config.h"

#include "tracker/tracker_http.h"

#include <iomanip>
#include <sstream>
#include <utility>

#include "net/address_list.h"
#include "torrent/connection_manager.h"
#include "torrent/exceptions.h"
#include "torrent/object_stream.h"
#include "torrent/net/http_stack.h"
#include "torrent/net/socket_address.h"
#include "torrent/runtime/network_config.h"
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

// TODO: Make sure stopped events are finished before deleting a torrent. (disown the request)
// TODO: If scrape fails, try next family.

TrackerHttp::TrackerHttp(const TrackerInfo& raw_info, int flags)
  : TrackerWorker(raw_info, utils::uri_can_scrape(raw_info.url) ? (flags | tracker::TrackerState::flag_scrapable) : flags) {

  m_get.reset(raw_info.url, nullptr);

  m_get.add_done_slot([this] { receive_done(); });
  m_get.add_failed_slot([this](const auto& str) { receive_signal_failed(str); });

  m_delay_scrape.slot() = [this] { delayed_send_scrape(); };

  auto [hostname, port] = net::parse_uri_host_port(raw_info.url);

  if (hostname.empty())
    return;

  auto [sa_inet, sa_inet6] = try_lookup_numeric(hostname, AF_UNSPEC);

  if (sa_inet)
    m_hostname_family = AF_INET;
  else if (sa_inet6)
    m_hostname_family = AF_INET6;
  else
    m_hostname_family = AF_UNSPEC;
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
    // LT_LOG("closing directly (already closed) : state:%s url:%s",
    //        option_as_string(OPTION_TRACKER_EVENT, state().latest_event()), info().url.c_str());

    m_slot_close();
    return;
  }

  LT_LOG("closing directly : state:%s family:%s url:%s",
         option_as_string(OPTION_TRACKER_EVENT, state().latest_event()), family_str(m_current_family), info().url.c_str());

  m_slot_close();

  m_get.close_and_cancel_callbacks(this_thread::thread());
  m_data.reset();
}

void
TrackerHttp::send_event(tracker::TrackerState::event_enum new_state) {
  close_directly();
  this_thread::scheduler()->erase(&m_delay_scrape);

  lock_and_set_latest_event(new_state);

  auto [current_family, next_family] = request_families();

  m_current_family = current_family;
  m_next_family    = next_family;

  m_last_success       = false;
  m_last_error_message = "";

  if (m_current_family == AF_UNSPEC) {
    LT_LOG("send event : no valid address family available : state:%s url:%s", option_as_string(OPTION_TRACKER_EVENT, new_state), info().url.c_str());
    return receive_failed("No valid address family available.");
  }

  send_event_unsafe(new_state);
}

void
TrackerHttp::send_scrape() {
  if (m_requested_scrape)
    return;

  m_requested_scrape = true;

  if (is_busy()) {
    LT_LOG("scrape requested, but tracker is busy : url:%s", info().url.c_str());
    return;
  }

  LT_LOG("scrape requested : url:%s", info().url.c_str());

  this_thread::scheduler()->update_wait_for_ceil_seconds(&m_delay_scrape, 10s);
}

void
TrackerHttp::send_event_unsafe(tracker::TrackerState::event_enum state) {
  // TODO: When retrying next protocol, recheck network_config (do in caller)

  auto params      = m_slot_parameters();
  auto request_url = request_announce_url(state, params, m_current_family);

  m_data = std::make_unique<std::stringstream>();

  m_get.try_wait_for_close();
  m_get.reset(request_url, m_data);

  if (m_current_family == AF_INET)
    m_get.use_ipv4();
  else if (m_current_family == AF_INET6)
    m_get.use_ipv6();
  else
    throw torrent::internal_error("TrackerHttp::send_event_unsafe() cannot send event, no valid address family to use.");

  LT_LOG("sending event : state:%s family:%s url:%s", option_as_string(OPTION_TRACKER_EVENT, state), family_str(m_current_family), info().url.c_str());
  LT_LOG_DUMP(request_url.c_str(), request_url.size(),
              "sending event : state:%s family:%s up_adj:%" PRIu64 " completed_adj:%" PRIu64 " left_adj:%" PRIu64,
              option_as_string(OPTION_TRACKER_EVENT, state), family_str(m_current_family),
              params.uploaded_adjusted, params.completed_adjusted, params.download_left);

  torrent::net_thread::http_stack()->start_get(m_get);
}

void
TrackerHttp::send_scrape_unsafe() {
  auto request_url = request_prefix(utils::uri_generate_scrape_url(info().url)).str();

  m_data = std::make_unique<std::stringstream>();

  m_get.try_wait_for_close();
  m_get.reset(request_url, m_data);

  if (m_current_family == AF_INET)
    m_get.use_ipv4();
  else if (m_current_family == AF_INET6)
    m_get.use_ipv6();
  else
    throw torrent::internal_error("TrackerHttp::send_scrape_unsafe() cannot send event, no valid address family to use.");

  LT_LOG("sending scrape : family:%s url:%s", family_str(m_current_family), info().url.c_str());
  LT_LOG_DUMP(request_url.c_str(), request_url.size(), "tracker scrape", 0);

  torrent::net_thread::http_stack()->start_get(m_get);
}

bool
TrackerHttp::send_next_family(bool scrape) {
  m_current_family = m_next_family;
  m_next_family    = AF_UNSPEC;

  if (m_current_family == AF_UNSPEC)
    return false;

  auto state = lock_and_latest_event();

  // TODO: If stopped state, don't bother if the other protocol hasn't been confirmed to work. (add vars to track this)

  if ((m_current_family == AF_INET && runtime::network_config()->is_block_ipv4()) ||
      (m_current_family == AF_INET6 && runtime::network_config()->is_block_ipv6()))
    return false;

  if (scrape)
    send_scrape_unsafe();
  else
    send_event_unsafe(state);

  return true;
}

// We delay scrape for 10 seconds after any tracker activity to ensure all callbacks are process
// before starting.
void
TrackerHttp::delayed_send_scrape() {
  if (is_busy())
    throw internal_error("TrackerHttp::delayed_send_scrape() called while busy");

  close_directly();

  lock_and_set_latest_event(tracker::TrackerState::EVENT_SCRAPE);

  auto [current_family, next_family] = request_families();

  m_current_family = current_family;
  m_next_family    = next_family;

  m_last_success       = false;
  m_last_error_message = "";

  if (m_current_family == AF_UNSPEC) {
    LT_LOG("send scrape : no valid address family available : url:%s", info().url.c_str());
    return;
  }

  send_scrape_unsafe();
}

std::stringstream
TrackerHttp::request_prefix(const std::string& url) {
  std::stringstream stream;
  stream.imbue(std::locale::classic());

  stream << url
         << (utils::uri_has_query(url) ? '&' : '?')
         << "info_hash=" << utils::copy_escape_html_str(info().info_hash);

  return stream;
}

std::string
TrackerHttp::request_announce_url(tracker::TrackerState::event_enum state, TrackerParameters params, int family) {
  auto tracker_id = this->tracker_id();
  auto local_id   = utils::copy_escape_html_str(info().local_id);

  auto s = request_prefix(info().url);

  s << "&peer_id=" << local_id
    << "&compact=1";

  if (info().key)
    s << "&key=" << std::hex << std::setw(8) << std::setfill('0') << info().key << std::dec;

  if (!tracker_id.empty())
    s << "&trackerid=" << utils::copy_escape_html_str(tracker_id);

  auto local_inet_address  = runtime::network_config()->local_inet_address_or_null();
  auto local_inet6_address = runtime::network_config()->local_inet6_address_or_null();

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

  s << "&port=" << runtime::listen_port()
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

std::tuple<int,int>
TrackerHttp::request_families() {
  bool is_block_ipv4  = runtime::network_config()->is_block_ipv4();
  bool is_block_ipv6  = runtime::network_config()->is_block_ipv6();
  bool is_prefer_ipv6 = runtime::network_config()->is_prefer_ipv6();

  if (m_hostname_family == AF_INET) {
    if (is_block_ipv4)
      return {AF_UNSPEC, AF_UNSPEC};

    return {AF_INET, AF_UNSPEC};
  }

  if (m_hostname_family == AF_INET6) {
    if (is_block_ipv6)
      return {AF_UNSPEC, AF_UNSPEC};

    return {AF_INET6, AF_UNSPEC};
  }

  // If both IPv4 and IPv6 are blocked, we cannot send the request.
  //
  // TODO: Properly handle this case without throwing an error.

  if (is_block_ipv4 && is_block_ipv6)
    return {AF_UNSPEC, AF_UNSPEC};

  // TODO: When sending 'stopped', only send to confirmed working protocol.
  // TODO: Don't constantly retry family that keeps failing.

  if (is_block_ipv4)
    return {AF_INET6, AF_UNSPEC};
  else if (is_block_ipv6)
    return {AF_INET, AF_UNSPEC};
  else if (is_prefer_ipv6)
    return {AF_INET6, AF_INET};
  else
    return {AF_INET, AF_INET6};
}

void
TrackerHttp::update_tracker_id(const std::string& id) {
  if (id.empty())
    return;

  if (m_current_tracker_id == id)
    return;

  set_tracker_id(id);
}

void
TrackerHttp::receive_done() {
  if (m_data == nullptr)
    throw internal_error("TrackerHttp::receive_done() called on an invalid object");

  LT_LOG("received reply : url:%s", info().url.c_str());

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
    this_thread::scheduler()->update_wait_for_ceil_seconds(&m_delay_scrape, 10s);
}

void
TrackerHttp::receive_signal_failed(const std::string& msg) {
  lock_and_clear_intervals();

  return receive_failed(msg);
}

void
TrackerHttp::receive_failed(const std::string& msg) {
  if (m_data == nullptr) {
    LT_LOG("received failure with no data : url:%s : %s", info().url.c_str(), msg.c_str());
    m_slot_failure(msg);
    return;
  }

  if (lt_log_is_valid(LOG_TRACKER_DUMP)) {
    std::string dump = m_data->str();
    LT_LOG_DUMP(dump.c_str(), dump.size(), "received failure", 0);
  }

  close_directly();

  if (state().latest_event() == tracker::TrackerState::EVENT_SCRAPE) {
    if (send_next_family(true))
      return;

    LT_LOG("received scrape failure : url:%s : %s", info().url.c_str(), msg.c_str());
    m_requested_scrape = false;
    m_slot_scrape_failure(msg);
    return;
  }

  if (send_next_family()) {
    m_last_success       = false;
    m_last_error_message = msg;
    return;
  }

  if (m_last_success) {
    LT_LOG("received failure : previous family succeeded : url:%s : %s", info().url.c_str(), msg.c_str());
    m_slot_success(AddressList());

  } else if (!m_last_error_message.empty()) {
    LT_LOG("received failure : previous family also failed : url:%s : %s /// %s", info().url.c_str(), msg.c_str(), m_last_error_message.c_str());
    m_slot_failure(msg + " /// " + m_last_error_message);

  } else {
    LT_LOG("received failure : url:%s : %s", info().url.c_str(), msg.c_str());
    m_slot_failure(msg);
  }

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

  AddressList address_list;
  bool        has_peer_fields = false;

  if (object.has_key("peers")) {
    try {
      // Due to some trackers sending the wrong type when no peers are
      // available, don't bork on it.
      if (object.get_key("peers").is_string())
        address_list.parse_address_compact(object.get_key_string("peers"));

      else if (object.get_key("peers").is_list())
        address_list.parse_address_normal(object.get_key_list("peers"));

    } catch (const bencode_error& e) {
      return receive_failed(e.what());
    }

    has_peer_fields = true;
  }

  if (object.has_key_string("peers6")) {
    address_list.parse_address_compact_ipv6(object.get_key_string("peers6"));
    has_peer_fields = true;
  }

  if (!has_peer_fields && state().latest_event() != tracker::TrackerState::EVENT_STOPPED)
    return receive_failed("No peers returned");

  close_directly();

  if (send_next_family()) {
    m_last_success       = true;
    m_last_error_message = "";

    m_slot_new_peers(std::move(address_list));
    return;
  }

  m_slot_success(std::move(address_list));
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

} // namespace torrent
