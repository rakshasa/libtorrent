#include "config.h"

#include "exceptions.h"
#include "http_get.h"
#include "http.h"

namespace torrent {

Http::Http() :
  m_get(new HttpGet()) {

  m_get->set_done(&m_done);
  m_get->set_failed(&m_failed);
}

Http::~Http() {
  if (m_get == NULL)
    throw internal_error("Http dtor called on an already destroyed object");

  delete m_get;

  m_get = NULL;
}

void Http::set_url(const std::string& url) {
  m_get->set_url(url);
}

void Http::set_out(std::ostream* out) {
  m_get->set_out(out);
}

const std::string& Http::get_url() const {
  return m_get->get_url();
}

void Http::start() {
  m_get->start();
}

void Http::close() {
  m_get->close();
}

}

