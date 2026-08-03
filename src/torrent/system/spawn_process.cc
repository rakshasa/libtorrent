#include "config.h"

#include "torrent/system/spawn_process.h"

#include <cassert>
#include <fcntl.h>
#include <spawn.h>
#include <unistd.h>

#include "torrent/exceptions.h"
#include "torrent/net/fd.h"
#include "torrent/system/types.h"

// Standard POSIX environment pointer
extern char** environ;

namespace torrent::system {

namespace {

std::pair<posix_spawn_file_actions_t, posix_spawnattr_t>
make_spawn_actions() {
  posix_spawn_file_actions_t actions{};
  posix_spawnattr_t          attr{};

  if (posix_spawn_file_actions_init(&actions) != 0)
    throw internal_error("SpawnProcess::make_spawn_actions() posix_spawn_file_actions_init failed.");

  if (posix_spawnattr_init(&attr) != 0)
    throw internal_error("SpawnProcess::make_spawn_actions() posix_spawnattr_init failed.");

  return {actions, attr};
}

void
add_open(posix_spawn_file_actions_t* actions, int fd, const char* path, int flags) {
  if (posix_spawn_file_actions_addopen(actions, fd, path, flags, 0) != 0)
    throw internal_error("SpawnProcess::add_open() posix_spawn_file_actions_addopen failed.");
}

std::pair<int, int>
add_redirects(posix_spawn_file_actions_t* actions, int log_fd, bool use_pipe) {
  int parent_fd = -1;
  int child_fd  = -1;

  add_open(actions, STDIN_FILENO, "/dev/null", O_RDWR);

  if (use_pipe) {
    fd_open_pipe(parent_fd, child_fd);

    posix_spawn_file_actions_adddup2(actions, child_fd, STDOUT_FILENO);
    posix_spawn_file_actions_addclose(actions, parent_fd);
    posix_spawn_file_actions_addclose(actions, child_fd);

  } else if (log_fd != -1) {
    posix_spawn_file_actions_adddup2(actions, log_fd, STDOUT_FILENO);

  } else {
    add_open(actions, STDOUT_FILENO, "/dev/null", O_RDWR);
  }

  if (log_fd != -1)
    posix_spawn_file_actions_adddup2(actions, log_fd, STDERR_FILENO);
  else
    add_open(actions, STDERR_FILENO, "/dev/null", O_RDWR);

  return {parent_fd, child_fd};
}

} // namespace anonymous

SpawnProcess::~SpawnProcess() {
  fd_close_and_clear(m_parent_fd);
  fd_close_and_clear(m_child_fd);
}

int
SpawnProcess::execute(const char* path, char* const argv[]) {
  assert(!m_background || !m_capture_output);

  // TODO: Caller should write to log fd before calling execute().

  // Try to avoid leaking open fds to the spawned process. Prefer POSIX_SPAWN_CLOEXEC_DEFAULT
  // (macOS-only) or posix_spawn_file_actions_addclosefrom_np (glibc >= 2.34, FreeBSD >= 13.1).
  //
  // Other platforms like musl libc, OpenBSD and NetBSD must rely on explicit O_CLOEXEC.

  auto [actions, attr] = make_spawn_actions();

  add_redirects(&actions, m_log_fd, m_capture_output);

  short spawn_flags = 0;

#if defined(POSIX_SPAWN_CLOEXEC_DEFAULT)
  spawn_flags |= POSIX_SPAWN_CLOEXEC_DEFAULT;
#elif defined(HAVE_POSIX_SPAWN_FILE_ACTIONS_ADDCLOSEFROM_NP)
  posix_spawn_file_actions_addclosefrom_np(&actions, 3);
#endif

  if (m_background) {
#ifdef POSIX_SPAWN_SETSID
    spawn_flags |= POSIX_SPAWN_SETSID;
#else
    spawn_flags |= POSIX_SPAWN_SETPGROUP;
    posix_spawnattr_setpgroup(&attr, 0);
#endif
  }

  posix_spawnattr_setflags(&attr, spawn_flags);

  int spawn_status = ::posix_spawnp(&m_child_pid, path, &actions, &attr, argv, environ);

  posix_spawn_file_actions_destroy(&actions);
  posix_spawnattr_destroy(&attr);

  if (spawn_status != 0)
    throw torrent::input_error("ExecFile::execute() posix_spawn failed: " + system::errno_enum_str(spawn_status));

  fd_close_and_clear(m_child_fd);

  // TODO: Write to log.
  if (m_background)
    return 0;

  return spawn_status;
}

std::string
SpawnProcess::capture_child_output() {
  assert(m_capture_output);

  std::string result;
  char        buffer[4096];

  while (true) {
    auto length = read(m_parent_fd, buffer, sizeof(buffer));

    if (length == 0)
      break;

    if (length == -1) {
      // TODO: This should throw input_error
      break;
    }

    result.append(buffer, length);
  }

  fd_close_and_clear(m_parent_fd);
  return result;
}

int
SpawnProcess::wait_for_child() {
  int status;

  while (::waitpid(m_child_pid, &status, 0) == -1) {
    switch (errno) {
    case EINTR:
      continue;
    case ECHILD:
      throw internal_error("ExecFile::execute() waitpid failed with ECHILD, child process not found.");
    case EINVAL:
      throw internal_error("ExecFile::execute() waitpid failed with EINVAL.");
    default:
      throw internal_error("ExecFile::execute() waitpid failed with unexpected error: " + std::string(std::strerror(errno)));
    }
  };

  // TODO: Write to log.

  return status;
}

} // namespace torrent::system
