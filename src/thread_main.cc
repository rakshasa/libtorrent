#include "config.h"

#include "thread_main.h"

#include "data/hash_queue.h"
#include "torrent/net/resolver.h"
#include "utils/instrumentation.h"

namespace torrent {

namespace main_thread {

system::Thread* thread()                                            { return ThreadMain::thread_base(); }
std::thread::id thread_id()                                         { return ThreadMain::thread_base()->thread_id(); }

void            callback(void* target, std::function<void ()>&& fn) { ThreadMain::thread_base()->callback(target, std::move(fn)); }
void            cancel_callback(void* target)                       { ThreadMain::thread_base()->cancel_callback(target); }
void            cancel_callback_and_wait(void* target)              { ThreadMain::thread_base()->cancel_callback_and_wait(target); }

// TODO: Not thread safe.
uint32_t        hash_queue_size()                                   { return ThreadMain::thread_main()->hash_queue()->size(); }

} // namespace main_thread

ThreadMain*     ThreadMain::m_thread_main{};
system::Thread* ThreadMain::m_thread_base{};

ThreadMain::~ThreadMain() {
  cleanup_thread();
}

void
ThreadMain::create_thread() {
  m_thread_main = new ThreadMain;
  m_thread_base = m_thread_main;

  m_thread_main->m_hash_queue = std::make_unique<HashQueue>();
}

void
ThreadMain::init_thread() {
  m_resolver = std::make_unique<net::Resolver>();
  m_state    = STATE_INITIALIZED;

  m_instrumentation_index = INSTRUMENTATION_POLLING_DO_POLL_MAIN - INSTRUMENTATION_POLLING_DO_POLL;

  init_thread_local();

  // We should only initialize things here that depend on main thread, as we want to call
  // 'init_thread()' before 'torrent::initalize()'.

  auto hash_work_signal = m_signal_bitfield.add_signal([this]() {
      return m_hash_queue->work();
    });

  m_hash_queue->slot_has_work() = [this, hash_work_signal](bool is_done) {
      send_event_signal(hash_work_signal, is_done);
    };
}

void
ThreadMain::cleanup_thread() {
  m_hash_queue.reset();

  m_thread_main = nullptr;
  m_thread_base = nullptr;
  m_self        = nullptr;
}

void
ThreadMain::call_events() {
  if (m_slot_do_work)
    m_slot_do_work();

  process_callbacks();
}

std::chrono::microseconds
ThreadMain::next_timeout() {
  return std::chrono::microseconds(10s);
}

} // namespace torrent
