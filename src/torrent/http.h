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
// Contact:  Jari Sundell <jaris@ifi.uio.no>
//
//           Skomakerveien 33
//           3185 Skoppum, NORWAY

#ifndef LIBTORRENT_HTTP_H
#define LIBTORRENT_HTTP_H

#include <string>
#include <iosfwd>
#include <sigc++/signal.h>

namespace torrent {

// Keep in mind that these objects get reused.
class Http {
 public:
  typedef sigc::signal0<void>                     Signal;
  typedef sigc::signal1<void, const std::string&> SignalString;
  typedef sigc::slot0<Http*>                      SlotFactory;

  Http() : m_userAgent("not_set"), m_stream(NULL) {}
  virtual ~Http();

  // Start must never throw on bad input. Calling start/stop on an
  // object in the wrong state should throw a torrent::internal_error.
  virtual void       start() = 0;
  virtual void       close() = 0;

  void               set_url(const std::string& url)      { m_url = url; }
  const std::string& get_url() const                      { return m_url; }

  // Make sure the output stream does not have any bad/failed bits set.
  void               set_stream(std::iostream* str)       { m_stream = str; }
  std::iostream*     get_stream()                         { return m_stream; }
  
  void               set_user_agent(const std::string& s) { m_userAgent = s; }
  const std::string& get_user_agent()                     { return m_userAgent; }

  Signal&            signal_done()                        { return m_signalDone; }
  SignalString&      signal_failed()                      { return m_signalFailed; }

  // Set the factory function that constructs and returns a valid Http* object.
  static  void       set_factory(const SlotFactory& f);

  // Guaranteed to return a valid object or throw a client_error.
  static  Http*      call_factory();

protected:
  std::string        m_url;
  std::string        m_userAgent;
  std::iostream*     m_stream;

  Signal             m_signalDone;
  SignalString       m_signalFailed;

private:
  // Disabled ctor. Do we want ref counting instead?
  Http(const Http&);
  void               operator = (const Http&);

  static SlotFactory m_factory;
};

}

#endif
