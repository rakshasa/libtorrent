#include "config.h"

#define __STDC_FORMAT_MACROS

#include "tracker/tracker_http.h"

#include <iomanip>
#include <sstream>
#include <rak/string_manip.h>

#include "net/address_list.h"
#include "torrent/connection_manager.h"
#include "torrent/download_info.h"
#include "torrent/exceptions.h"
#include "torrent/http.h"
#include "torrent/net/utils.h"
#include "torrent/net/socket_address.h"
#include "torrent/object_stream.h"
#include "torrent/tracker_list.h"
#include "torrent/utils/log.h"
#include "torrent/utils/option_strings.h"

#include "globals.h"
#include "manager.h"

#define LT_LOG_TRACKER_REQUESTS(log_fmt, ...)                             \
  lt_log_print_info(LOG_TRACKER_REQUESTS, m_parent->info(), "tracker_http", log_fmt, __VA_ARGS__);

#define LT_LOG_TRACKER_DUMP(log_level, log_dump_data, log_dump_size, log_fmt, ...)                   \
  lt_log_print_info_dump(LOG_TRACKER_DUMP, log_dump_data, log_dump_size, m_parent->info(), "tracker_http", log_fmt, __VA_ARGS__);

namespace torrent {

static std::string
generate_scrape_url(std::string url) {
  size_t delim_slash = url.rfind('/');

  if (delim_slash == std::string::npos || url.find("/announce", delim_slash) != delim_slash)
    throw internal_error("Tried to make scrape url from invalid url.");

  return url.replace(delim_slash, sizeof("/announce") - 1, "/scrape");
}

TrackerHttp::TrackerHttp(TrackerList* parent, const std::string& url, int flags) :
  TrackerWorker(parent, url, flags),

  m_get(Http::slot_factory()()),
  m_data(NULL) {

  m_get->signal_done().emplace_back(std::bind(&TrackerHttp::receive_done, this));
  m_get->signal_failed().emplace_back(std::bind(&TrackerHttp::receive_signal_failed, this, std::placeholders::_1));

  // Haven't considered if this needs any stronger error detection,
  // can dropping the '?' be used for malicious purposes?
  size_t delim_options = url.rfind('?');

  m_dropDeliminator = delim_options != std::string::npos &&
    url.find('/', delim_options) == std::string::npos;

  // Check the url if we can use scrape.
  size_t delim_slash = url.rfind('/');

  if (delim_slash != std::string::npos &&
      url.find("/announce", delim_slash) == delim_slash)
    m_flags |= flag_can_scrape;
}

TrackerHttp::~TrackerHttp() {
  delete m_get;
  delete m_data;
}

bool
TrackerHttp::is_busy() const {
  return m_data != NULL;
}

void
TrackerHttp::request_prefix(std::stringstream* stream, const std::string& url) {
  char hash[61];

  *rak::copy_escape_html(m_parent->info()->hash().begin(),
                         m_parent->info()->hash().end(), hash) = '\0';
  *stream << url
          << (m_dropDeliminator ? '&' : '?')
          << "info_hash=" << hash;
}

void
TrackerHttp::send_event(TrackerState::event_enum new_state) {
  close_directly();

  if (m_parent == NULL)
    throw internal_error("TrackerHttp::send_state(...) does not have a valid m_parent.");

  set_latest_event(new_state);

  std::stringstream s;
  s.imbue(std::locale::classic());

  char localId[61];

  auto info = m_parent->info();
  auto tracker_id = this->tracker_id();

  request_prefix(&s, url());

  *rak::copy_escape_html(info->local_id().begin(), info->local_id().end(), localId) = '\0';

  s << "&peer_id=" << localId;

  if (m_parent->key())
    s << "&key=" << std::hex << std::setw(8) << std::setfill('0') << m_parent->key() << std::dec;

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

  if (info->is_compact())
    s << "&compact=1";

  if (m_parent->numwant() >= 0 && new_state != TrackerState::EVENT_STOPPED)
    s << "&numwant=" << m_parent->numwant();

  if (manager->connection_manager()->listen_port())
    s << "&port=" << manager->connection_manager()->listen_port();

  uint64_t uploaded_adjusted = info->uploaded_adjusted();
  uint64_t completed_adjusted = info->completed_adjusted();
  uint64_t download_left = info->slot_left()();

  s << "&uploaded=" << uploaded_adjusted
    << "&downloaded=" << completed_adjusted
    << "&left=" << download_left;

  switch(new_state) {
  case TrackerState::EVENT_STARTED:
    s << "&event=started";
    break;
  case TrackerState::EVENT_STOPPED:
    s << "&event=stopped";
    break;
  case TrackerState::EVENT_COMPLETED:
    s << "&event=completed";
    break;
  default:
    break;
  }

  m_data = new std::stringstream();

  std::string request_url = s.str();

  LT_LOG_TRACKER_DUMP(DEBUG, request_url.c_str(), request_url.size(),
                      "Tracker HTTP request: state:%s up_adj:%" PRIu64 " completed_adj:%" PRIu64 " left_adj:%" PRIu64 ".",
                      option_as_string(OPTION_TRACKER_EVENT, new_state),
                      uploaded_adjusted, completed_adjusted, download_left);

  m_get->set_url(request_url);
  m_get->set_stream(m_data);
  m_get->set_timeout(2 * 60);

  m_get->start();
}

void
TrackerHttp::send_scrape() {
  if (m_data != NULL)
    return;

  set_latest_event(TrackerState::EVENT_SCRAPE);

  std::stringstream s;
  s.imbue(std::locale::classic());

  request_prefix(&s, generate_scrape_url(url()));

  m_data = new std::stringstream();

  std::string request_url = s.str();

  LT_LOG_TRACKER_DUMP(DEBUG, request_url.c_str(), request_url.size(), "Tracker HTTP scrape.", 0);

  m_get->set_url(request_url);
  m_get->set_stream(m_data);
  m_get->set_timeout(2 * 60);

  m_get->start();
}

void
TrackerHttp::close() {
  if (m_data == NULL)
    return;

  LT_LOG_TRACKER_REQUESTS("Tracker HTTP request cancelled: state:%s url:%s.",
                 option_as_string(OPTION_TRACKER_EVENT, state().latest_event()), url().c_str());

  close_directly();
}

void
TrackerHttp::disown() {
  if (m_data == NULL)
    return;

  LT_LOG_TRACKER_REQUESTS("Tracker HTTP request disowned: state:%s url:%s.",
                 option_as_string(OPTION_TRACKER_EVENT, state().latest_event()), url().c_str());

  m_get->set_delete_self();
  m_get->set_delete_stream();
  m_get->signal_done().clear();
  m_get->signal_failed().clear();

  // Allocate this dynamically, so that we don't need to do this here.
  m_get = Http::slot_factory()();
  m_data = NULL;
}

tracker_enum
TrackerHttp::type() const {
  return TRACKER_HTTP;
}

void
TrackerHttp::close_directly() {
  if (m_data == NULL)
    return;

  m_get->close();
  m_get->set_stream(NULL);

  delete m_data;
  m_data = NULL;
}

void
TrackerHttp::receive_done() {
  if (m_data == NULL)
    throw internal_error("TrackerHttp::receive_done() called on an invalid object");

  if (lt_log_is_valid(LOG_TRACKER_DEBUG)) {
    std::string dump = m_data->str();
    LT_LOG_TRACKER_DUMP(DEBUG, dump.c_str(), dump.size(), "Tracker HTTP reply.", 0);
  }

  Object b;
  *m_data >> b;

  // Temporarily reset the interval
  //
  // TODO: This might be causing an issue with too frequent tracker requests.
  clear_intervals();

  if (m_data->fail()) {
    std::string dump = m_data->str();
    return receive_failed("Could not parse bencoded data: " + rak::sanitize(rak::striptags(dump)).substr(0,99));
  }

  if (!b.is_map())
    return receive_failed("Root not a bencoded map");

  if (b.has_key("failure reason")) {
    if (state().latest_event() != TrackerState::EVENT_SCRAPE)
      process_failure(b);

    return receive_failed("Failure reason \"" +
                         (b.get_key("failure reason").is_string() ?
                          b.get_key_string("failure reason") :
                          std::string("failure reason not a string"))
                         + "\"");
  }

  // If no failures, set intervals to defaults prior to processing

  if (state().latest_event() == TrackerState::EVENT_SCRAPE)
    process_scrape(b);
  else
    process_success(b);
}

void
TrackerHttp::receive_signal_failed(std::string msg) {
  clear_intervals();
  return receive_failed(msg);
}

void
TrackerHttp::receive_failed(std::string msg) {
  if (lt_log_is_valid(LOG_TRACKER_DEBUG)) {
    std::string dump = m_data->str();
    LT_LOG_TRACKER_DUMP(DEBUG, dump.c_str(), dump.size(), "Tracker HTTP failed.", 0);
  }

  close_directly();

  if (state().latest_event() == TrackerState::EVENT_SCRAPE)
    m_slot_scrape_failure(msg);
  else
    m_slot_failure(msg);
}

void
TrackerHttp::process_failure(const Object& object) {
  if (object.has_key_string("tracker id"))
    update_tracker_id(object.get_key_string("tracker id"));

  auto tracker_state = state();

  if (object.has_key_value("interval"))
    tracker_state.set_normal_interval(object.get_key_value("interval"));

  if (object.has_key_value("min interval"))
    tracker_state.set_min_interval(object.get_key_value("min interval"));

  if (object.has_key_value("complete") && object.has_key_value("incomplete")) {
    tracker_state.m_scrape_complete = std::max<int64_t>(object.get_key_value("complete"), 0);
    tracker_state.m_scrape_incomplete = std::max<int64_t>(object.get_key_value("incomplete"), 0);
    tracker_state.m_scrape_time_last = cachedTime.seconds();
  }

  if (object.has_key_value("downloaded"))
    tracker_state.m_scrape_downloaded = std::max<int64_t>(object.get_key_value("downloaded"), 0);

  set_state(tracker_state);
}

void
TrackerHttp::process_success(const Object& object) {
  if (object.has_key_string("tracker id"))
    update_tracker_id(object.get_key_string("tracker id"));

  auto tracker_state = state();

  if (object.has_key_value("interval"))
    tracker_state.set_normal_interval(object.get_key_value("interval"));
  else
    tracker_state.set_normal_interval(TrackerState::default_normal_interval);

  if (object.has_key_value("min interval"))
    tracker_state.set_min_interval(object.get_key_value("min interval"));
  else
    tracker_state.set_min_interval(TrackerState::default_min_interval);

  if (object.has_key_value("complete") && object.has_key_value("incomplete")) {
    tracker_state.m_scrape_complete = std::max<int64_t>(object.get_key_value("complete"), 0);
    tracker_state.m_scrape_incomplete = std::max<int64_t>(object.get_key_value("incomplete"), 0);
    tracker_state.m_scrape_time_last = cachedTime.seconds();
  }

  if (object.has_key_value("downloaded"))
    tracker_state.m_scrape_downloaded = std::max<int64_t>(object.get_key_value("downloaded"), 0);

  set_state(tracker_state);

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

    } catch (bencode_error& e) {
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

  if (!files.has_key_map(m_parent->info()->hash().str()))
    return receive_failed("Tracker scrape replay did not contain infohash.");

  const Object& stats = files.get_key(m_parent->info()->hash().str());

  auto tracker_state = state();

  if (stats.has_key_value("complete"))
    tracker_state.m_scrape_complete = std::max<int64_t>(stats.get_key_value("complete"), 0);

  if (stats.has_key_value("incomplete"))
    tracker_state.m_scrape_incomplete = std::max<int64_t>(stats.get_key_value("incomplete"), 0);

  if (stats.has_key_value("downloaded"))
    tracker_state.m_scrape_downloaded = std::max<int64_t>(stats.get_key_value("downloaded"), 0);

  set_state(tracker_state);

  LT_LOG_TRACKER_REQUESTS("Tracker scrape for %u torrents: complete:%u incomplete:%u downloaded:%u.",
                          files.as_map().size(), tracker_state.m_scrape_complete, tracker_state.m_scrape_incomplete, tracker_state.m_scrape_downloaded);

  close_directly();
  m_slot_scrape_success();
}

}
