#include "config.h"

#include "torrent/exceptions.h"
#include "http.h"

namespace torrent {

Http::SlotFactory Http::m_factory;

Http::~Http() {
}

void
Http::set_factory(const SlotFactory& f) {
  m_factory = f;
}  

Http*
Http::call_factory() {
  if (m_factory.empty())
    throw client_error("Http factory not set");

  Http* h = m_factory();

  if (h == NULL)
    throw client_error("Http factory returned a NULL object");

  return h;
}

}

