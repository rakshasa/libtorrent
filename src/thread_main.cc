#include "config.h"

#include "thread_main.h"

#include "data/hash_queue.h"
#include "torrent/net/resolver.h"
#include "torrent/runtime/network_config.h"
#include "torrent/runtime/network_manager.h"
#include "torrent/system/callbacks.h"
#include "utils/instrumentation.h"

namespace torrent {

namespace main_thread {

system::Thread* thread()                                                                 { return ThreadMain::thread_base(); }
std::thread::id thread_id()                                                              { return ThreadMain::thread_base()->thread_id(); }

void            callback(std::function<void ()>&& fn)                                    { ThreadMain::thread_base()->callback(std::move(fn)); }
void            callback(system::callback_id& id, std::function<void ()>&& fn)           { ThreadMain::thread_base()->callback(id, std::move(fn)); }
void            callback_interrupt(std::function<void ()>&& fn)                          { ThreadMain::thread_base()->callback_interrupt(std::move(fn)); }
void            callback_interrupt(system::callback_id& id, std::function<void ()>&& fn) { ThreadMain::thread_base()->callback_interrupt(id, std::move(fn)); }

void            cancel_callback(system::callback_id& id)                                 { ThreadMain::thread_base()->cancel_callback(id); }
void            cancel_callback_and_wait(system::callback_id& id)                        { ThreadMain::thread_base()->cancel_callback_and_wait(id); }

void            set_client_callback(std::function<void()> fn)                            { ThreadMain::thread_main()->set_client_callback(std::move(fn)); }

// TODO: Not thread safe.
uint32_t        hash_queue_size()                                                        { return ThreadMain::thread_main()->hash_queue()->size(); }

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

  runtime::network_config()->subscribe_to_changes(this, [this]() {
      cancel_callback(this);

      callback(this, []() {
          runtime::network_manager()->listen_restart();

          // TODO: Restart DHT.
        });
    });
}

void
ThreadMain::cleanup_thread() {
  runtime::network_config()->unsubscribe_from_changes(this);

  m_hash_queue.reset();

  m_thread_main = nullptr;
  m_thread_base = nullptr;
  m_self        = nullptr;
}

void
ThreadMain::set_client_callback(std::function<void()> fn) {
  m_slot_client_callback = std::move(fn);
}

void
ThreadMain::call_events() {
  if (m_slot_client_callback)
    m_slot_client_callback();

  process_callbacks();
}

std::chrono::microseconds
ThreadMain::next_timeout() {
  return std::chrono::microseconds(10s);
}

} // namespace torrent
