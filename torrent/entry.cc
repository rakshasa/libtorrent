#include "config.h"

#include "entry.h"
#include "content/content_file.h"

namespace torrent {

uint64_t
Entry::get_size() {
  return m_entry->get_size();
}

Entry::Range
Entry::get_range() {
  return m_entry->get_range();
}

unsigned int
Entry::get_completed() {
  return m_entry->get_completed();
}

Entry::Priority
Entry::get_priority() {
  return (Priority)m_entry->get_priority();
}

// Relative to root of torrent.
const std::string
Entry::get_path() {
  return m_entry->get_path().path(false);
}

void
Entry::set_priority(Priority p) {
  m_entry->set_priority(p);
}

}
