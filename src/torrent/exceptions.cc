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

#include "config.h"

#include <cerrno>
#include <cstring>
#include <netdb.h>
#include <sstream>
#include <unistd.h>

#ifdef HAVE_BACKTRACE
#include <execinfo.h>
#endif

#include "exceptions.h"

namespace torrent {

// Use actual functions, instead of inlined, for the ctor of
// exceptions. This allows us to create breakpoints at throws. This is
// limited to rarely thrown exceptions.

void communication_error::initialize(const std::string& msg) { m_msg = msg; }
void storage_error::initialize(const std::string& msg) { m_msg = msg; }
void resource_error::initialize(const std::string& msg) { m_msg = msg; }
void input_error::initialize(const std::string& msg) { m_msg = msg; }

const char*
connection_error::what() const throw() {
  return std::strerror(m_errno);
}

const char*
address_info_error::what() const throw() {
  return ::gai_strerror(m_errno);
}

void
internal_error::initialize(const std::string& msg) {
  m_msg = msg;

  std::stringstream output;

#ifdef HAVE_BACKTRACE
  void* stackPtrs[20];

  // Print the stack and exit.
  int stackSize = ::backtrace(stackPtrs, 20);
  char** stackStrings = backtrace_symbols(stackPtrs, stackSize);

  for (int i = 0; i < stackSize; ++i)
    output << stackStrings[i] << std::endl;

#else
  output << "Stack dump not enabled." << std::endl;
#endif

  m_backtrace = output.str();
}

}
