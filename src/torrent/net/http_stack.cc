#include "config.h"

#include "torrent/net/http_stack.h"

#include <cassert>

#include "net/curl_stack.h"
#include "net/thread_net.h"
#include "torrent/exceptions.h"
#include "torrent/utils/thread.h"

namespace torrent::net {

HttpStack::HttpStack(utils::Thread* thread)
  : m_thread(thread),
    m_stack(std::make_unique<net::CurlStack>()) {
}

HttpStack::~HttpStack() = default;

}
