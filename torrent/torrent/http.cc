#include "config.h"

#include "torrent/exceptions.h"
#include "http.h"

namespace torrent {

static Http*
http_null_factory() {
  return NULL;
}

Http::SlotFactory Http::m_factory(sigc::ptr_fun(&http_null_factory));

void
Http::set_factory(const SlotFactory& f) {
  m_factory = f;
}  

Http*
Http::call_factory() {
  Http* h = m_factory();

  if (h == NULL)
    throw client_error("Http factory returned a NULL object");

  return h;
}

}

