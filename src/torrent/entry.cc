#include "config.h"

#include "exceptions.h"
#include "entry.h"
#include "content/content_file.h"

namespace torrent {

uint64_t
Entry::get_size() {
  return ((ContentFile*)m_entry)->get_size();
}

uint32_t
Entry::get_completed() {
  return ((ContentFile*)m_entry)->get_completed();
}

uint32_t
Entry::get_chunk_begin() {
  return ((ContentFile*)m_entry)->get_range().first;
}

uint32_t
Entry::get_chunk_end() {
  return ((ContentFile*)m_entry)->get_range().second;
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

const Entry::Path&
Entry::get_path_list() {
  return ((ContentFile*)m_entry)->get_path().list();
}

void
Entry::set_path_list(const Path& l) {
  if (l.empty())
    throw client_error("Tried to set empty path list for Entry");

  ((ContentFile*)m_entry)->get_path().list() = l;
}

void
Entry::set_priority(Priority p) {
  ((ContentFile*)m_entry)->set_priority(p);
}

}
