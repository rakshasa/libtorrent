#include "config.h"

#include "process/worker/thread_worker.h"

// #include "manager.h"
// #include "runtime_manager.h"
// #include "data/hash_queue.h"
// #include "process/process_worker.h"
// #include "torrent/data/file_manager.h"
// #include "torrent/net/resolver.h"
// #include "torrent/runtime/network_config.h"
// #include "torrent/runtime/network_manager.h"
// #include "torrent/runtime/socket_manager.h"
// #include "torrent/system/callbacks.h"
// // #include "torrent/system/ipc/factory.h"
// // #include "torrent/system/ipc/router.h"
// #include "torrent/tracker/dht_controller.h"
// #include "utils/instrumentation.h"

namespace torrent::worker_thread {

system::Thread* thread()                                                                 { return process::worker::ThreadWorker::thread_base(); }
std::thread::id thread_id()                                                              { return process::worker::ThreadWorker::thread_base()->thread_id(); }

void            callback(std::function<void ()>&& fn)                                    { thread()->callback(std::move(fn)); }
void            callback(system::callback_id& id, std::function<void ()>&& fn)           { thread()->callback(id, std::move(fn)); }
void            callback_interrupt(std::function<void ()>&& fn)                          { thread()->callback_interrupt(std::move(fn)); }
void            callback_interrupt(system::callback_id& id, std::function<void ()>&& fn) { thread()->callback_interrupt(id, std::move(fn)); }

void            cancel_callback(system::callback_id& id)                                 { thread()->cancel_callback(id); }
void            cancel_callback_and_wait(system::callback_id& id)                        { thread()->cancel_callback_and_wait(id); }

} // namespace torrent::worker_thread

namespace torrent::process::worker {

ThreadWorker* ThreadWorker::m_thread_worker{};

ThreadWorker::~ThreadWorker() {
  cleanup_thread();
}

void
ThreadWorker::create_thread() {
  m_thread_worker = new ThreadWorker;
}

void
ThreadWorker::init_thread() {
  // m_resolver = std::make_unique<net::Resolver>();
  m_state    = STATE_INITIALIZED;

  init_thread_local();

  // We should only initialize things here that depend on worker thread, as we want to call
  // 'init_thread()' before 'torrent::initalize()'.
}

void
ThreadWorker::init_after_setup() {
}

void
ThreadWorker::cleanup_thread() {
  m_thread_worker = nullptr;
  m_self          = nullptr;
}

void
ThreadWorker::call_events() {
  process_callbacks();
}

std::chrono::microseconds
ThreadWorker::next_timeout() {
  return std::chrono::microseconds(10s);
}

} // namespace torrent
