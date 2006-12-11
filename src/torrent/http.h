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

#ifndef LIBTORRENT_HTTP_H
#define LIBTORRENT_HTTP_H

#include <string>
#include <iosfwd>
#include <sigc++/signal.h>
#include <torrent/common.h>

namespace torrent {

// The client should set the user agent to something like
// "CLIENT_NAME/CLIENT_VERSION/LIBRARY_VERSION".

// Keep in mind that these objects get reused.
class LIBTORRENT_EXPORT Http {
 public:
  typedef sigc::signal0<void>                     Signal;
  typedef sigc::signal1<void, const std::string&> SignalString;
  typedef sigc::slot0<Http*>                      SlotFactory;

  Http() : m_stream(NULL), m_timeout(0) {}
  virtual ~Http();

  // Start must never throw on bad input. Calling start/stop on an
  // object in the wrong state should throw a torrent::internal_error.
  virtual void       start() = 0;
  virtual void       close() = 0;

  const std::string& url() const                          { return m_url; }
  void               set_url(const std::string& url)      { m_url = url; }

  // Make sure the output stream does not have any bad/failed bits set.
  std::iostream*     stream()                             { return m_stream; }
  void               set_stream(std::iostream* str)       { m_stream = str; }
  
  uint32_t           timeout() const                      { return m_timeout; }
  void               set_timeout(uint32_t seconds)        { m_timeout = seconds; }

  Signal&            signal_done()                        { return m_signalDone; }
  SignalString&      signal_failed()                      { return m_signalFailed; }

  // Set the factory function that constructs and returns a valid Http* object.
  static void        set_factory(const SlotFactory& f);

  // Guaranteed to return a valid object or throw a internal_error. The
  // caller takes ownership of the returned object. Is there any
  // interest in making a destructor slot?
  static Http*       call_factory();

protected:
  std::string        m_url;
  std::iostream*     m_stream;
  uint32_t           m_timeout;

  Signal             m_signalDone;
  SignalString       m_signalFailed;

private:
  Http(const Http&);
  void               operator = (const Http&);

  static SlotFactory m_factory;
};

}

#endif
