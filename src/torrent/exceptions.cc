#include "config.h"

#include <cstring>
#include <netdb.h>
#include <sstream>

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
internal_error::what() const noexcept {
  return m_msg.c_str();
}

const char*
input_error::what() const noexcept {
  return m_msg.c_str();
}

const char*
resource_error::what() const noexcept {
  return m_msg.c_str();
}

const char*
storage_error::what() const noexcept {
  return m_msg.c_str();
}

const char*
communication_error::what() const noexcept {
  return m_msg.c_str();
}

const char*
connection_error::what() const noexcept {
  return std::strerror(m_errno);
}

const char*
address_info_error::what() const noexcept {
  return ::gai_strerror(m_errno);
}

void
internal_error::initialize(const std::string& msg) {
  m_msg = msg;

#ifdef HAVE_BACKTRACE
  void* stack_ptrs[20];
  int stack_size = ::backtrace(stack_ptrs, 20);
  char** stack_symbol_names = ::backtrace_symbols(stack_ptrs, stack_size);

  if (stack_symbol_names == nullptr) {
    m_backtrace = "backtrace_symbols failed";
    return;
  }

  std::stringstream output;

  for (int i = 0; i < stack_size; ++i) {
    if (stack_symbol_names[i] != nullptr && stack_symbol_names[i] > (void*)0x1000)
      output << stack_symbol_names[i] << '\n';
    else
      output << "stack_symbol: nullptr" << '\n';
  }

  m_backtrace = output.str();
  ::free(stack_symbol_names);

#else
  m_backtrace = "stack dump not enabled";
#endif
}

} // namespace torrent
