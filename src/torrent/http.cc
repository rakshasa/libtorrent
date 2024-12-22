#include "config.h"

#include <iostream>

#include "torrent/exceptions.h"
#include "torrent/http.h"
#include "torrent/utils/log.h"
#include "utils/functional.h"

namespace torrent {

Http::slot_http Http::m_factory;

Http::~Http() {
}

void
Http::trigger_done() {
  if (signal_done().empty())
    lt_log_print(LOG_TRACKER_INFO, "Disowned tracker done: url:'%s'.", m_url.c_str());

  bool should_delete_self = (m_flags & flag_delete_self);
  bool should_delete_stream = (m_flags & flag_delete_stream);

  utils::slot_list_call(signal_done());

  if (should_delete_stream) {
    delete m_stream;
    m_stream = NULL;
  }

  if (should_delete_self)
    delete this;
}

void
Http::trigger_failed(const std::string& message) {
  if (signal_done().empty())
    lt_log_print(LOG_TRACKER_INFO, "Disowned tracker failed: url:'%s'.", m_url.c_str());

  bool should_delete_self = (m_flags & flag_delete_self);
  bool should_delete_stream = (m_flags & flag_delete_stream);

  utils::slot_list_call(signal_failed(), message);

  if (should_delete_stream) {
    delete m_stream;
    m_stream = NULL;
  }

  if (should_delete_self)
    delete this;
}

}

