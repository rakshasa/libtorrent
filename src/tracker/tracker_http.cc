#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sigc++/signal.h>
#include <sstream>
#include <fstream>

#include "torrent/exceptions.h"
#include "torrent/http.h"
#include "settings.h"
#include "tracker_http.h"
#include "tracker_info.h"

// STOPPED is only sent once, if the connections fails then we stop trying.
// START has a retry of [very short]
// COMPLETED/NONE has a retry of [short]

// START success leads to NONE and [interval]

namespace torrent {

TrackerHttp::TrackerHttp(TrackerInfo* info, const std::string& url) :
  m_get(Http::call_factory()),
  m_data(NULL),
  m_info(info),
  m_url(url) {

  m_get->set_user_agent(PACKAGE "/" VERSION);

  m_get->signal_done().connect(sigc::mem_fun(*this, &TrackerHttp::receive_done));
  m_get->signal_failed().connect(sigc::mem_fun(*this, &TrackerHttp::receive_failed));
}

TrackerHttp::~TrackerHttp() {
  delete m_get;
  delete m_data;
}

void
TrackerHttp::send_state(TrackerInfo::State state, uint64_t down, uint64_t up, uint64_t left) {
  close();

  if (m_info == NULL || m_info->get_me() == NULL)
    throw internal_error("TrackerHttp::send_state(...) does not have a valid m_info or m_me");

  if (m_info->get_me()->get_id().length() != 20 ||
      m_info->get_me()->get_port() == 0 ||
      m_info->get_hash().length() != 20)
    throw internal_error("Send state with TrackerHttp with bad hash, id or port");

  std::stringstream s;

  s << m_url << "?info_hash=";
  escape_string(m_info->get_hash(), s);

  s << "&peer_id=";
  escape_string(m_info->get_me()->get_id(), s);

  if (!m_info->get_key().empty())
    s << "&key=" << m_info->get_key();

  if (!m_trackerId.empty()) {
    s << "&trackerid=";
    escape_string(m_trackerId, s);
  }

  if (m_info->get_me()->get_dns().length())
    s << "&ip=" << m_info->get_me()->get_dns();

  if (m_info->get_compact())
    s << "&compact=1";

  if (m_info->get_numwant() >= 0)
    s << "&numwant=" << m_info->get_numwant();

  s << "&port=" << m_info->get_me()->get_port()
    << "&uploaded=" << up
    << "&downloaded=" << down
    << "&left=" << left;

  switch(state) {
  case TrackerInfo::STARTED:
    s << "&event=started";
    break;
  case TrackerInfo::STOPPED:
    s << "&event=stopped";
    break;
  case TrackerInfo::COMPLETED:
    s << "&event=completed";
    break;
  default:
    break;
  }

  m_data = new std::stringstream();

  m_get->set_url(s.str());
  m_get->set_stream(m_data);

  m_get->start();
}

void
TrackerHttp::close() {
  if (m_data == NULL)
    return;

  m_get->close();
  m_get->set_stream(NULL);

  delete m_data;
  m_data = NULL;
}

void
TrackerHttp::escape_string(const std::string& src, std::ostream& stream) {
  // TODO: Correct would be to save the state.
  stream << std::hex << std::uppercase;

  for (std::string::const_iterator itr = src.begin(); itr != src.end(); ++itr)
    if ((*itr >= 'A' && *itr <= 'Z') ||
	(*itr >= 'a' && *itr <= 'z') ||
	(*itr >= '0' && *itr <= '9') ||
	*itr == '-')
      stream << *itr;
    else
      stream << '%' << ((unsigned char)*itr >> 4) << ((unsigned char)*itr & 0xf);

  stream << std::dec << std::nouppercase;
}

void
TrackerHttp::receive_done() {
  if (m_data == NULL)
    throw internal_error("TrackerHttp::receive_done() called on an invalid object");

  Bencode b;
  *m_data >> b;

  if (m_data->fail()) {
    return receive_failed("Could not parse bencoded data");
  } else {
    close();
    m_signalDone.emit(b);
  }
}

void
TrackerHttp::receive_failed(std::string msg) {
  close();

  m_signalFailed.emit(msg);
}

}
