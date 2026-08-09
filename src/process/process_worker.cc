#include "config.h"

#include "process/process_worker.h"

#include <future>
#include <unistd.h>
#include <iostream>

#include "process/worker/thread_worker.h"
#include "torrent/exceptions.h"
#include "torrent/system/types.h"
#include "torrent/system/ipc/control_fd.h"
#include "torrent/system/ipc/factory.h"
#include "torrent/utils/log.h"

#define LT_LOG_PARENT(log_fmt, ...)                                     \
  lt_log_print(LOG_SYSTEM_IPC, "parent->ipc: " log_fmt, __VA_ARGS__);
#define LT_LOG_PARENT_EVENTS(log_fmt, ...)                              \
  lt_log_print(LOG_SYSTEM_IPC_EVENTS, "parent->ipc: " log_fmt, __VA_ARGS__);

#define LT_LOG_CHILD(log_fmt, ...)                                      \
  lt_log_print(LOG_SYSTEM_IPC, "worker->ipc: " log_fmt, __VA_ARGS__);
#define LT_LOG_CHILD_EVENTS(log_fmt, ...)                               \
  lt_log_print(LOG_SYSTEM_IPC_EVENTS, "worker->ipc: " log_fmt, __VA_ARGS__);

namespace torrent::process {

namespace {

void
start_control_watchdog(int fd) {
  // TODO: gracefully exit before killing the process.

  [[maybe_unused]] auto watchdog_thread = std::async(std::launch::async, [fd]() {
      // while (true) {
      //   // if (!system::ipc::ControlFd::check_is_alive(fd)) {
      //   //   std::this_thread::sleep_for(5s);

      //   //   std::cout << "ProcessWorker: control fd closed, exiting process." << std::endl;

      //   //   // TODO: Wait for child to exit.
      //   //   ::exit(1);
      //   // }

      //   // // std::this_thread::sleep_for(10min);
      //   // std::this_thread::sleep_for(10s);
      // }

      system::ipc::ControlFd::wait_is_alive(fd);

      // Allow main thread a chance to exit gracefully before killing the process.
      std::this_thread::sleep_for(100ms);

      // std::this_thread::sleep_for(10s);

      LT_LOG_CHILD("ProcessWorker::watchdog: control fd closed, exiting process.", 0);

      ::exit(1);
    });
}

} // namespace anonymous

ProcessWorker::ProcessWorker() = default;
ProcessWorker::~ProcessWorker() = default;

// TODO: Change EventFd to be the interrupt handler for poll? And add an option to make it cross-process.

void
ProcessWorker::spawn(std::function<void()> init_child_fn) {
  m_factory = std::make_unique<system::ipc::RouterFactory>();

  m_factory->initialize(1 * 4096);

  pid_t pid = ::fork();

  if (pid == -1)
    throw internal_error("fork() failed: " + std::string(strerror(errno)));

  if (pid != 0)
    return;

  init_child_fn();
  init_child_process();

  worker::ThreadWorker::thread_worker()->event_loop();

  m_router->close_fds();

  // int control_fd = m_router->control_fd().file_descriptor();

  // while (system::ipc::ControlFd::check_is_alive(control_fd))
  //   std::this_thread::sleep_for(100ms);

  // std::this_thread::sleep_for(1s);

  // std::this_thread::sleep_for(5s);

  LT_LOG_CHILD("ProcessWorker: control fd closed, exiting child process.", 0);

  ::exit(0);
}

void
ProcessWorker::init_parent_process() {
  LT_LOG_PARENT("ProcessWorker::init_parent_process() called.", 0);

  m_router = m_factory->create_parent_router();
  m_factory.reset();

  // m_router->control_fd().register_interrupt_handler([]() {
  //     LT_LOG_PARENT("ProcessWorker::init_parent_process(): control fd interrupt received.", 0);
  //   });

  m_router->control_fd().register_message_handler([](auto msg) {
      LT_LOG_PARENT("ProcessWorker::init_parent_process(): control fd message received : %s", msg.c_str());
    });

  m_router->control_fd().register_closed_handler([router = m_router.get()](int error_code) {
      LT_LOG_PARENT("ProcessWorker::init_parent_process(): control fd closed with error code: %s", system::errno_enum(error_code));
    });

  m_router->control_fd().register_shutdown_handler([](bool graceful) {
      LT_LOG_PARENT("ProcessWorker::init_parent_process(): control fd shutdown received. graceful: %i", (int)graceful);

      throw shutdown_exception();
    });

  // std::cout << "PARENT: started: fd." << std::endl;

  // auto parent_handler = new ParentHandler{};
  // parent_handler->id = 1;

  // router->register_handler(1,
  //                          [parent_handler](void* data, uint32_t size) { parent_handler->on_read(data, size); },
  //                          [parent_handler](void* data, uint32_t size) { parent_handler->on_error(data, size); });

  // auto handler_1 = parent_handler->create_new_channel(router);
  // auto handler_2 = parent_handler->create_new_channel(router);

  m_router->open_fds();

  LT_LOG_PARENT("ProcessWorker::init_parent_process() finished.", 0);
}

void
ProcessWorker::init_child_process() {
  LT_LOG_CHILD("ProcessWorker::init_child_process() called.", 0);

  m_router = m_factory->create_child_router();
  m_factory.reset();

  // TODO: Register SIGSEGV/ETC handlers, send backtrace to parent process.

  m_router->control_fd().register_message_handler([](auto msg) {
      LT_LOG_CHILD("ProcessWorker::init_child_process(): control fd message received : %s", msg.c_str());
    });

  m_router->control_fd().register_closed_handler([router = m_router.get()](int error_code) {
      LT_LOG_CHILD("ProcessWorker::init_child_process(): control fd closed with error code: %s", system::errno_enum(error_code));
    });

  m_router->control_fd().register_shutdown_handler([](bool graceful) {
      LT_LOG_CHILD("ProcessWorker::init_child_process(): control fd shutdown received. graceful: %i", (int)graceful);

      throw shutdown_exception();
    });

  start_control_watchdog(m_router->keepalive_fd());

  worker::ThreadWorker::create_thread();
  worker::ThreadWorker::thread_worker()->init_thread();

  m_router->open_fds();

  LT_LOG_CHILD("ProcessWorker::init_child_process() finished.", 0);
}

} // namespace torrent::process


// #include <sys/prctl.h>
// #include <signal.h>

// // Called immediately inside the child process:
// prctl(PR_SET_PDEATHSIG, SIGTERM);

// // Mitigate the race condition where the parent died right before prctl invoked:
// if (getppid() == 1) {
//     exit(1);
// }


// BSD/macos: Doesn't help avoid using a thread, only removes the need for a watchdog fd.

// #include <sys/event.h>
// #include <unistd.h>

// struct kevent kev;
// int kq = kqueue();
// EV_SET(&kev, getppid(), EVFILT_PROC, EV_ADD | EV_ENABLE, NOTE_EXIT, 0, NULL);
// kevent(kq, &kev, 1, NULL, 0, NULL);
// // You can now block on kevent() to await the parent's NOTE_EXIT event
