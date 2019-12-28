#import "torrent/utils/thread_base.h"

class test_thread : public torrent::thread_base {
public:
  enum test_state {
    TEST_NONE,
    TEST_PRE_START,
    TEST_PRE_STOP,
    TEST_STOP
  };

  static const int test_flag_pre_stop       = 0x1;
  static const int test_flag_long_timeout   = 0x2;

  static const int test_flag_acquire_global = 0x10;
  static const int test_flag_has_global     = 0x20;

  static const int test_flag_do_work   = 0x100;
  static const int test_flag_pre_poke  = 0x200;
  static const int test_flag_post_poke = 0x400;

  test_thread();

  int     test_state() const { return m_test_state; }
  bool    is_state(int state) const { return m_state == state; }
  bool    is_test_state(int state) const { return m_test_state == state; }
  bool    is_test_flags(int flags) const { return (m_test_flags & flags) == flags; }
  bool    is_not_test_flags(int flags) const { return !(m_test_flags & flags); }

  auto    name() const -> const char* { return "test_thread"; }

  void    init_thread();

  void    set_pre_stop() { __sync_or_and_fetch(&m_test_flags, test_flag_pre_stop); }
  void    set_acquire_global() { __sync_or_and_fetch(&m_test_flags, test_flag_acquire_global); }

  void    set_test_flag(int flags) { __sync_or_and_fetch(&m_test_flags, flags); }

private:
  void    call_events();
  int64_t next_timeout_usec() { return (m_test_flags & test_flag_long_timeout) ? (10000 * 1000) : (100 * 1000); }

  int     m_test_state lt_cacheline_aligned;
  int     m_test_flags lt_cacheline_aligned;
};

struct thread_management_type {
  thread_management_type();
  ~thread_management_type();
};

#define SETUP_THREAD()                                                  \
  thread_management_type thread_management;                             \
  torrent::thread_disk* thread_disk = new torrent::thread_disk();       \
  thread_disk->init_thread();

#define CLEANUP_THREAD()                                                \
  CPPUNIT_ASSERT(wait_for_true(std::bind(&torrent::thread_base::is_inactive, thread_disk))); \
  delete thread_disk;
