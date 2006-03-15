// libTorrent - BitTorrent library
// Copyright (C) 2005-2006, Jari Sundell
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
#include <rak/functional.h>
#include <rak/string_manip.h>

#include "torrent/connection_manager.h"
#include "torrent/exceptions.h"
#include "torrent/http.h"
#include "tracker_http.h"

#include "globals.h"
#include "manager.h"

namespace torrent {

TrackerHttp::TrackerHttp(DownloadInfo* info, const std::string& url) :
  TrackerBase(info, url),

  m_get(Http::call_factory()),
  m_data(NULL) {

  m_get->signal_done().connect(sigc::mem_fun(*this, &TrackerHttp::receive_done));
  m_get->signal_failed().connect(sigc::mem_fun(*this, &TrackerHttp::receive_failed));
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
TrackerHttp::send_state(DownloadInfo::State state, uint64_t down, uint64_t up, uint64_t left) {
  close();

  if (m_info == NULL)
    throw internal_error("TrackerHttp::send_state(...) does not have a valid m_info");

  if (m_info->local_id().length() != 20 ||
      m_info->hash().length() != 20)
    throw internal_error("Send state with TrackerHttp with bad hash or id");

  std::stringstream s;

  s << m_url
    << "?info_hash=" << rak::copy_escape_html(m_info->hash())
    << "&peer_id=" << rak::copy_escape_html(m_info->local_id())
    << "&key=" << std::hex << std::setw(8) << std::setfill('0') << m_info->key() << std::dec;

  if (!m_trackerId.empty())
    s << "&trackerid=" << rak::copy_escape_html(m_trackerId);

  const rak::socket_address* localAddress = rak::socket_address::cast_from(manager->socket_manager()->local_address());

  if (localAddress->family() == rak::socket_address::af_inet &&
      !localAddress->sa_inet()->is_address_any())
    s << "&ip=" << localAddress->address_str();

  if (m_info->compact())
    s << "&compact=1";

  if (m_info->numwant() >= 0)
    s << "&numwant=" << m_info->numwant();

  s << "&port=" << m_info->port()
    << "&uploaded=" << up
    << "&downloaded=" << down
    << "&left=" << left;

  switch(state) {
  case DownloadInfo::STARTED:
    s << "&event=started";
    break;
  case DownloadInfo::STOPPED:
    s << "&event=stopped";
    break;
  case DownloadInfo::COMPLETED:
    s << "&event=completed";
    break;
  default:
    break;
  }

  m_data = new std::stringstream();

  m_get->set_url(s.str());
  m_get->set_stream(m_data);
  m_get->set_timeout(2 * 60);

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

TrackerHttp::Type
TrackerHttp::type() const {
  return TRACKER_HTTP;
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
			 (b.get_key("failure reason").is_string() ?
			  b.get_key("failure reason").as_string() :
			  std::string("failure reason not a string"))
			 + "\"");

  if (b.has_key("interval") && b.get_key("interval").is_value())
    m_slotSetInterval(b.get_key("interval").as_value());
  
  if (b.has_key("min interval") && b.get_key("min interval").is_value())
    m_slotSetMinInterval(b.get_key("min interval").as_value());

  if (b.has_key("tracker id") && b.get_key("tracker id").is_string())
    m_trackerId = b.get_key("tracker id").as_string();

  if (b.has_key("complete") && b.get_key("complete").is_value() &&
      b.has_key("incomplete") && b.get_key("incomplete").is_value()) {
    m_scrapeComplete   = std::max<int64_t>(b.get_key("complete").as_value(), 0);
    m_scrapeIncomplete = std::max<int64_t>(b.get_key("incomplete").as_value(), 0);

    m_scrapeTimeLast = cachedTime;
  }

  if (b.has_key("downloaded") && b.get_key("downloaded").is_value())
    m_scrapeDownloaded = std::max<int64_t>(b.get_key("downloaded").as_value(), 0);

  AddressList l;

  try {
    if (b.get_key("peers").is_string())
      parse_address_compact(&l, b.get_key("peers").as_string());
    else
      parse_address_normal(&l, b.get_key("peers").as_list());

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

inline rak::socket_address
TrackerHttp::parse_address(const Bencode& b) {
  rak::socket_address sa;
  sa.clear();

  if (!b.is_map())
    return sa;

  if (!b.has_key("ip") || !b.get_key("ip").is_string() ||
      !sa.set_address_str(b.get_key("ip").as_string()))
    return sa;

  if (!b.has_key("port") || !b.get_key("port").is_value() ||
      b.get_key("port").as_value() <= 0 || b.get_key("port").as_value() >= (1 << 16))
    return sa;

  sa.set_port(b.get_key("port").as_value());

  return sa;
}

void
TrackerHttp::parse_address_normal(AddressList* l, const Bencode::List& b) {
  std::for_each(b.begin(), b.end(), rak::on(std::ptr_fun(&TrackerHttp::parse_address), address_list_add_address(l)));
}

void
TrackerHttp::parse_address_compact(AddressList* l, const std::string& s) {
  if (sizeof(const SocketAddressCompact) != 6)
    throw internal_error("TrackerHttp::parse_address_compact(...) bad struct size.");

  std::copy(reinterpret_cast<const SocketAddressCompact*>(s.c_str()),
	    reinterpret_cast<const SocketAddressCompact*>(s.c_str() + s.size() - s.size() % sizeof(SocketAddressCompact)),
	    std::back_inserter(*l));
}

}
