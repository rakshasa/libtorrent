// libTorrent - BitTorrent library
// Copyright (C) 2005-2011, Jari Sundell
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
#include <list>
#include lt_tr1_functional
#include <torrent/common.h>

namespace torrent {

// The client should set the user agent to something like
// "CLIENT_NAME/CLIENT_VERSION/LIBRARY_VERSION".

// Keep in mind that these objects get reused.
class LIBTORRENT_EXPORT Http {
 public:
  typedef std::function<void ()>                   slot_void;
  typedef std::function<void (const std::string&)> slot_string;
  typedef std::function<Http* (void)>              slot_http;

  typedef std::list<slot_void>   signal_void;
  typedef std::list<slot_string> signal_string;

  static const int flag_delete_self   = 0x1;
  static const int flag_delete_stream = 0x2;

  Http() : m_flags(0), m_stream(NULL), m_timeout(0) {}
  virtual ~Http();

  // Start must never throw on bad input. Calling start/stop on an
  // object in the wrong state should throw a torrent::internal_error.
  virtual void       start() = 0;
  virtual void       close() = 0;

  int                flags() const                        { return m_flags; }

  void               set_delete_self()                    { m_flags |= flag_delete_self; }
  void               set_delete_stream()                  { m_flags |= flag_delete_stream; }

  const std::string& url() const                          { return m_url; }
  void               set_url(const std::string& url)      { m_url = url; }

  // Make sure the output stream does not have any bad/failed bits set.
  std::iostream*     stream()                             { return m_stream; }
  void               set_stream(std::iostream* str)       { m_stream = str; }
  
  uint32_t           timeout() const                      { return m_timeout; }
  void               set_timeout(uint32_t seconds)        { m_timeout = seconds; }

  // The owner of the Http object must close it as soon as possible
  // after receiving the signal, as the implementation may allocate
  // limited resources during its lifetime.
  signal_void&       signal_done()                        { return m_signal_done; }
  signal_string&     signal_failed()                      { return m_signal_failed; }

  // Guaranteed to return a valid object or throw a internal_error. The
  // caller takes ownership of the returned object.
  static slot_http&  slot_factory()                       { return m_factory; }

protected:
  void               trigger_done();
  void               trigger_failed(const std::string& message);

  int                m_flags;
  std::string        m_url;
  std::iostream*     m_stream;
  uint32_t           m_timeout;

  signal_void        m_signal_done;
  signal_string      m_signal_failed;

private:
  Http(const Http&);
  void               operator = (const Http&);

  static slot_http   m_factory;
};

}

#endif
