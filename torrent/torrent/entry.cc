#include "config.h"

#include "entry.h"
#include "content/content_file.h"

namespace torrent {

uint64_t
Entry::get_size() {
  return ((ContentFile*)m_entry)->get_size();
}

Entry::Range
Entry::get_range() {
  return ((ContentFile*)m_entry)->get_range();
}

uint32_t
Entry::get_completed() {
  return ((ContentFile*)m_entry)->get_completed();
}

Entry::Priority
Entry::get_priority() {
  return (Priority)((ContentFile*)m_entry)->get_priority();
}

// Relative to root of torrent.
std::string
Entry::get_path() {
  return ((ContentFile*)m_entry)->get_path().path(false);
}

void
Entry::set_priority(Priority p) {
  ((ContentFile*)m_entry)->set_priority(p);
}

}
