#include "config.h"

#include "process/process_worker.h"

#include <future>
#include <unistd.h>
#include <iostream>

#include "process/worker/thread_worker.h"
#include "torrent/exceptions.h"
#include "torrent/system/ipc/control_fd.h"
#include "torrent/system/ipc/factory.h"

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
      // std::this_thread::sleep_for(100ms);

      std::this_thread::sleep_for(10s);


      std::cout << "ProcessWorker::watchdog: control fd closed, exiting process." << std::endl;

      ::exit(1);
    });
}

} // namespace anonymous

ProcessWorker::ProcessWorker() = default;
ProcessWorker::~ProcessWorker() = default;

// TODO: Change EventFd to be the interrupt handler for poll? And add an option to make it cross-process.

void
ProcessWorker::spawn() {
  system::ipc::RouterFactory factory;

  factory.initialize(1 * 4096);

  pid_t pid = ::fork();

  if (pid == -1)
    throw internal_error("fork() failed: " + std::string(strerror(errno)));

  if (pid != 0) {
    // TODO: Add pid to sig handler to kill child process on crash.

    m_router = factory.create_parent_router();
    return;
  }

  m_router = factory.create_child_router();

  start_control_watchdog(m_router->keepalive_fd());

  worker::ThreadWorker::create_thread();
  worker::ThreadWorker::thread_worker()->init_thread();

  m_router->open_fds();

  worker::ThreadWorker::thread_worker()->event_loop();

  m_router->close_fds();

  // int control_fd = m_router->control_fd().file_descriptor();

  // while (system::ipc::ControlFd::check_is_alive(control_fd))
  //   std::this_thread::sleep_for(100ms);

  // std::this_thread::sleep_for(1s);

  // std::this_thread::sleep_for(5s);

  std::cout << "ProcessWorker: control fd closed, exiting child process." << std::endl;

  ::exit(0);
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
