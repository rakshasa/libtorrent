// libTorrent - BitTorrent library
// Copyright (C) 2005, Jari Sundell
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
// In addition, as a special exception, the copyright holders give
// permission to link the code of portions of this program with the
// OpenSSL library under certain conditions as described in each
// individual source file, and distribute linked combinations
// including the two.
//
// You must obey the GNU General Public License in all respects for
// all of the code used other than OpenSSL.  If you modify file(s)
// with this exception, you may extend this exception to your version
// of the file(s), but you are not obligated to do so.  If you do not
// wish to do so, delete this exception statement from your version.
// If you delete this exception statement from all source files in the
// program, then also delete it here.
//
// Contact:  Jari Sundell <jaris@ifi.uio.no>
//
//           Skomakerveien 33
//           3185 Skoppum, NORWAY

#include "config.h"

#include <iomanip>
#include <sstream>
#include <sigc++/bind.h>

#include "torrent/exceptions.h"
#include "torrent/http.h"
#include "tracker_http.h"
#include "tracker_info.h"

namespace torrent {

TrackerHttp::TrackerHttp(TrackerInfo* info, const std::string& url) :
  TrackerBase(info, url),

  m_get(Http::call_factory()),
  m_data(NULL) {

  m_get->signal_done().connect(sigc::mem_fun(*this, &TrackerHttp::receive_done));
  m_get->signal_failed().connect(sigc::mem_fun(*this, &TrackerHttp::receive_failed));

  m_taskTimeout.set_iterator(taskScheduler.end());
  m_taskTimeout.set_slot(sigc::bind(sigc::mem_fun(*this, &TrackerHttp::receive_failed), "Connection stalled"));
}

TrackerHttp::~TrackerHttp() {
  if (taskScheduler.is_scheduled(&m_taskTimeout))
    throw internal_error("TrackerHttp::~TrackerHttp() called but m_taskTimeout still scheduled.");

  delete m_get;
  delete m_data;
}

bool
TrackerHttp::is_busy() const {
  return m_data != NULL;
}

void
TrackerHttp::send_state(TrackerInfo::State state, uint64_t down, uint64_t up, uint64_t left) {
  close();

  if (m_info == NULL)
    throw internal_error("TrackerHttp::send_state(...) does not have a valid m_info");

  if (m_info->get_local_id().length() != 20 ||
      m_info->get_local_address().get_port() == 0 ||
      m_info->get_hash().length() != 20)
    throw internal_error("Send state with TrackerHttp with bad hash, id or port");

  std::stringstream s;

  s << m_url << "?info_hash=";
  escape_string(m_info->get_hash(), s);

  s << "&peer_id=";
  escape_string(m_info->get_local_id(), s);

  s << "&key=" << std::hex << std::setw(8) << std::setfill('0') << m_info->get_key() << std::dec;

  if (!m_trackerId.empty()) {
    s << "&trackerid=";
    escape_string(m_trackerId, s);
  }

  if (!m_info->get_local_address().is_address_any())
    s << "&ip=" << m_info->get_local_address().get_address();

  if (m_info->get_compact())
    s << "&compact=1";

  if (m_info->get_numwant() >= 0)
    s << "&numwant=" << m_info->get_numwant();

  s << "&port=" << m_info->get_local_address().get_port()
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

//   taskScheduler.insert(&m_taskTimeout, (Timer::cache() + m_info->http_timeout() * 1000000).round_seconds());
}

void
TrackerHttp::close() {
  if (m_data == NULL)
    return;

  m_get->close();
  m_get->set_stream(NULL);

  delete m_data;
  m_data = NULL;

  taskScheduler.erase(&m_taskTimeout);
}

TrackerHttp::Type
TrackerHttp::get_type() const {
  return TRACKER_HTTP;
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

  if (m_data->fail())
    return receive_failed("Could not parse bencoded data");

  if (!b.is_map())
    return receive_failed("Root not a bencoded map");

  if (b.has_key("failure reason"))
    return receive_failed("Failure reason \"" +
			 (b["failure reason"].is_string() ?
			  b["failure reason"].as_string() :
			  std::string("failure reason not a string"))
			 + "\"");

  if (b.has_key("interval") && b["interval"].is_value())
    m_slotSetInterval(b["interval"].as_value());
  
  if (b.has_key("min interval") && b["min interval"].is_value())
    m_slotSetMinInterval(b["min interval"].as_value());

  if (b.has_key("tracker id") && b["tracker id"].is_string())
    m_trackerId = b["tracker id"].as_string();

  AddressList l;

  try {
    if (b["peers"].is_string())
      parse_address_compact(l, b["peers"].as_string());
    else
      parse_address_normal(l, b["peers"].as_list());

  } catch (bencode_error& e) {
    return receive_failed(e.what());
  }

  close();
  m_slotSuccess(this, &l);
}

void
TrackerHttp::receive_failed(std::string msg) {
  // Does the order matter?
  close();
  m_slotFailed(this, msg);
}

inline SocketAddress
TrackerHttp::parse_address(const Bencode& b) {
  SocketAddress sa;

  if (!b.is_map())
    return SocketAddress();

  for (Bencode::Map::const_iterator itr = b.as_map().begin(); itr != b.as_map().end(); ++itr)
    // We ignore the "peer id" field, we don't need it.
    if (itr->first == "ip" && itr->second.is_string())
      sa.set_address(itr->second.as_string());

    else if (itr->first == "port" && itr->second.is_value())
      sa.set_port(itr->second.as_value());

  return sa.is_valid() ? sa : SocketAddress();
}

void
TrackerHttp::parse_address_normal(AddressList& l, const Bencode::List& b) {
  std::transform(b.begin(), b.end(), std::back_inserter(l), std::ptr_fun(&TrackerHttp::parse_address));
}  

void
TrackerHttp::parse_address_compact(AddressList& l, const std::string& s) {
  if (sizeof(const SocketAddressCompact) != 6)
    throw internal_error("TrackerHttp::parse_address_compact(...) bad struct size.");

  std::copy(reinterpret_cast<const SocketAddressCompact*>(s.c_str()),
	    reinterpret_cast<const SocketAddressCompact*>(s.c_str() + s.size() - s.size() % sizeof(SocketAddressCompact)),
	    std::back_inserter(l));
}

}
