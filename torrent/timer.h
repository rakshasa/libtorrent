#ifndef LIBTORRENT_TIMER_H
#define LIBTORRENT_TIMER_H

#include <stdint.h>
#include <sys/time.h>

namespace torrent {

// Don't convert negative Timer to timeval and then back to Timer, that will bork.
class Timer {
 public:
  Timer() : m_time(0) {}
  Timer(int64_t usec) : m_time(usec) {}
  Timer(timeval tv) : m_time((int64_t)(unsigned)tv.tv_sec * 1000000 + (int64_t)(unsigned)tv.tv_usec % 1000000) {}

  int64_t usec() const {
    return m_time;
  }

  double seconds() const {
    return (double)m_time / 1000000.0;
  }

  timeval tval() const {
    timeval t;
    t.tv_sec = m_time / 1000000;
    t.tv_usec = m_time % 1000000;

    return t;
  }

  Timer operator - (const Timer& t) const {
    return Timer(m_time - t.m_time);
  }

  Timer operator + (const Timer& t) const {
    return Timer(m_time + t.m_time);
  }

  Timer operator -= (const Timer& t) {
    m_time -= t.m_time;

    return *this;
  }

  Timer operator -= (uint64_t t) {
    m_time -= t;

    return *this;
  }

  Timer operator += (const Timer& t) {
    m_time += t.m_time;

    return *this;
  }

  Timer operator += (uint64_t t) {
    m_time += t;

    return *this;
  }

  static Timer current() {
    timeval t;
    gettimeofday(&t, 0);

    return Timer(t);
  }

  // Cached time, updated in the beginning of torrent::work call.
  // Don't use outside socket_base read/write/except or Service::service.
  static Timer cache() {
    return Timer(m_cache);
  }

  static void update() {
    m_cache = Timer::current().usec();
  }

  bool operator < (const Timer& t) const {
    return m_time < t.m_time;
  }

  bool operator > (const Timer& t) const {
    return m_time > t.m_time;
  }

 private:
  int64_t m_time;

  // Instantiated in torrent.cc
  static int64_t m_cache;
};

}

#endif // LIBTORRENT_TIMER_H
