#include "config.h"

#include "torrent/system/system.h"

#include <cerrno>
#include <cstdio>

#include "torrent/exceptions.h"
#include "torrent/system/thread.h"

namespace torrent::system {

void
cancel_callback_and_wait(callback_id& id, Thread* thread1, Thread* thread2) {
  if (this_thread::thread() == thread1) {
    thread1->cancel_callback_and_wait(id, thread2);
    return;
  }

  if (this_thread::thread() == thread2) {
    thread2->cancel_callback_and_wait(id, thread1);
    return;
  }

  throw internal_error("system::cancel_callback_and_wait() neither thread is self.");
}

const char*
errno_enum(int status) {
  // The Open Group Base Specifications Issue 6
  switch (status) {
  case 0:               return "0";
  case E2BIG:           return "E2BIG";
  case EACCES:          return "EACCES";
  case EADDRINUSE:      return "EADDRINUSE";
  case EADDRNOTAVAIL:   return "EADDRNOTAVAIL";
  case EAFNOSUPPORT:    return "EAFNOSUPPORT";
  case EAGAIN:          return "EAGAIN";
  case EALREADY:        return "EALREADY";
  case EBADF:           return "EBADF";
  case EBADMSG:         return "EBADMSG";
  case EBUSY:           return "EBUSY";
  case ECANCELED:       return "ECANCELED";
  case ECHILD:          return "ECHILD";
  case ECONNABORTED:    return "ECONNABORTED";
  case ECONNREFUSED:    return "ECONNREFUSED";
  case ECONNRESET:      return "ECONNRESET";
  case EDEADLK:         return "EDEADLK";
  case EDESTADDRREQ:    return "EDESTADDRREQ";
  case EDOM:            return "EDOM";
  case EDQUOT:          return "EDQUOT";
  case EEXIST:          return "EEXIST";
  case EFAULT:          return "EFAULT";
  case EFBIG:           return "EFBIG";
  case EHOSTUNREACH:    return "EHOSTUNREACH";
  case EIDRM:           return "EIDRM";
  case EILSEQ:          return "EILSEQ";
  case EINPROGRESS:     return "EINPROGRESS";
  case EINTR:           return "EINTR";
  case EINVAL:          return "EINVAL";
  case EIO:             return "EIO";
  case EISCONN:         return "EISCONN";
  case EISDIR:          return "EISDIR";
  case ELOOP:           return "ELOOP";
  case EMFILE:          return "EMFILE";
  case EMLINK:          return "EMLINK";
  case EMSGSIZE:        return "EMSGSIZE";
#if defined(EMULTIHOP)
  case EMULTIHOP:       return "EMULTIHOP";
#endif
  case ENAMETOOLONG:    return "ENAMETOOLONG";
  case ENETDOWN:        return "ENETDOWN";
  case ENETRESET:       return "ENETRESET";
  case ENETUNREACH:     return "ENETUNREACH";
  case ENFILE:          return "ENFILE";
  case ENOBUFS:         return "ENOBUFS";
  case ENODATA:         return "ENODATA";
  case ENODEV:          return "ENODEV";
  case ENOENT:          return "ENOENT";
  case ENOEXEC:         return "ENOEXEC";
  case ENOLCK:          return "ENOLCK";
  case ENOLINK:         return "ENOLINK";
  case ENOMEM:          return "ENOMEM";
  case ENOMSG:          return "ENOMSG";
  case ENOPROTOOPT:     return "ENOPROTOOPT";
  case ENOSPC:          return "ENOSPC";
  case ENOSR:           return "ENOSR";
  case ENOSTR:          return "ENOSTR";
  case ENOSYS:          return "ENOSYS";
  case ENOTCONN:        return "ENOTCONN";
  case ENOTDIR:         return "ENOTDIR";
  case ENOTEMPTY:       return "ENOTEMPTY";
  case ENOTSOCK:        return "ENOTSOCK";
  case ENOTTY:          return "ENOTTY";
  case ENXIO:           return "ENXIO";
  case EOPNOTSUPP:      return "EOPNOTSUPP";
  case EOVERFLOW:       return "EOVERFLOW";
  case EPERM:           return "EPERM";
  case EPIPE:           return "EPIPE";
  case EPROTO:          return "EPROTO";
  case EPROTONOSUPPORT: return "EPROTONOSUPPORT";
  case EPROTOTYPE:      return "EPROTOTYPE";
  case ERANGE:          return "ERANGE";
  case EROFS:           return "EROFS";
  case ESPIPE:          return "ESPIPE";
  case ESRCH:           return "ESRCH";
  case ESTALE:          return "ESTALE";
  case ETIME:           return "ETIME";
  case ETIMEDOUT:       return "ETIMEDOUT";
  case ETXTBSY:         return "ETXTBSY";
  case EXDEV:           return "EXDEV";

  default:
    // Handle potentially duplicate error numbers here.
    switch (status) {
    case ENOTSUP:       return "ENOTSUP";
    case EWOULDBLOCK:   return "EWOULDBLOCK";
    default:
      static thread_local char buffer[16];
      std::snprintf(buffer, sizeof(buffer), "E%d", status);

      return buffer;
    };
  };
}

std::string
errno_enum_str(int status) {
  return errno_enum(status);
}

} // namespace torrent::system

