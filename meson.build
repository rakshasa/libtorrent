project(
  'libtorrent',
  'cpp',
  version: '0.15.4',
  meson_version: '>=1.3.0',
  default_options: ['cpp_std=c++17', 'warning_level=1'],
)

if get_option('b_ndebug') not in ['true', 'if-release']
  add_project_arguments('-DDEBUG', language: 'cpp')
endif

cdata = configuration_data()

cdata.set('HAVE_CONFIG_H', 1)
cdata.set_quoted('VERSION', meson.project_version())
cdata.set_quoted('PEER_NAME', '-lt0F03-')
cdata.set_quoted('PEER_VERSION', 'lt\\x0F\\x03')

cc = meson.get_compiler('cpp')
if cc.has_function_attribute('visibility:default')
  cdata.set('SUPPORT_ATTRIBUTE_VISIBILITY', 1)
endif

cdata.set('IS_@0@_ENDIAN'.format(host_machine.endian().to_upper()), 1)

cdata.set('LT_INSTRUMENTATION', 1)

if cc.has_header_symbol('new', 'std::hardware_constructive_interference_size')
  cdata.set('LT_SMP_CACHE_BYTES', 'std::hardware_constructive_interference_size')
else
  cdata.set('LT_SMP_CACHE_BYTES', '128')
endif

cdata.set('lt_cacheline_aligned', 'alignas(LT_SMP_CACHE_BYTES)')
if cc.sizeof('void*') == 8
  cdata.set('DEFAULT_ADDRESS_SPACE_SIZE', 4096)
else
  cdata.set('DEFAULT_ADDRESS_SPACE_SIZE', 1024)
endif

foreach h : ['epoll', 'inotify']
  if cc.has_header('sys/@0@.h'.format(h), required: get_option(h))
    cdata.set('USE_@0@'.format(h.to_upper()), 1)
  endif
endforeach

foreach f : ['madvise', 'mincore']
  if cc.has_header_symbol('sys/mman.h', f, required: get_option(f))
    cdata.set('USE_@0@'.format(f.to_upper()), 1)
  endif
endforeach

if cc.has_function('__builtin_popcount')
  cdata.set('USE_BUILTIN_POPCOUNT', 1)
endif

if cc.has_function('posix_fadvise')
  cdata.set('USE_POSIX_FADVISE', 1)
endif

if cc.has_function('fallocate')
  cdata.set('USE_FALLOCATE', 1)
elif host_machine.system() == 'darwin'
  cdata.set('SYS_DARWIN', 1)
elif cc.has_function('posix_fallocate')
  cdata.set('USE_POSIX_FALLOCATE', 1)
endif

if cdata.has('USE_MINCORE')
  if cc.compiles('#include <sys/mman.h>\nint main(){mincore(0,0,(unsigned char*)0);}', werror: true)
    cdata.set('USE_MINCORE_UNSIGNED', 1)
  else
    cdata.set('USE_MINCORE_UNSIGNED', 0)
  endif
endif

if cc.compiles('#include <pthread.h>\nint main(){pthread_setname_np("foo");}')
  cdata.set('HAS_PTHREAD_SETNAME_NP_DARWIN', 1)
elif cc.compiles('#include <pthread.h>\nint main(){pthread_t t;pthread_setname_np(t,"foo");}')
  cdata.set('HAS_PTHREAD_SETNAME_NP_GENERIC', 1)
endif

if cc.has_header('sys/statvfs.h', required: get_option('statvfs'))
  cdata.set('HAVE_SYS_STATVFS_H', 1)
  cdata.set('FS_STAT_FD', 'fstatvfs(fd, &m_stat) == 0')
  cdata.set('FS_STAT_FN', 'statvfs(fn, &m_stat) == 0')
  cdata.set('FS_STAT_STRUCT', 'struct statvfs')
  cdata.set('FS_STAT_SIZE_TYPE', 'unsigned long')
  cdata.set('FS_STAT_COUNT_TYPE', 'fsblkcnt_t')
  cdata.set('FS_STAT_BLOCK_SIZE', '(m_stat.f_frsize)')
elif cc.has_header('sys/statfs.h', required: get_option('statfs'))
  cdata.set('HAVE_SYS_STATFS_H', 1)
  cdata.set('HAVE_SYS_VFS_H', 1)
  cdata.set('FS_STAT_FD', 'fstatfs(fd, &m_stat) == 0')
  cdata.set('FS_STAT_FN', 'statfs(fn, &m_stat) == 0')
  cdata.set('FS_STAT_STRUCT', 'struct statfs')
  cdata.set('FS_STAT_SIZE_TYPE', 'long')
  cdata.set('FS_STAT_COUNT_TYPE', 'long')
  cdata.set('FS_STAT_BLOCK_SIZE', '(m_stat.f_bsize)')
else
  cdata.set('FS_STAT_FD', '(errno = ENOSYS) == 0')
  cdata.set('FS_STAT_FN', '(errno = ENOSYS) == 0')
  cdata.set('FS_STAT_STRUCT', 'struct {blocksize_type f_bsize;blockcount_type f_bavail;}')
  cdata.set('FS_STAT_SIZE_TYPE', 'int')
  cdata.set('FS_STAT_COUNT_TYPE', 'int')
  cdata.set('FS_STAT_BLOCK_SIZE', '(4096)')
endif

if get_option('execinfo').enabled()
  if cc.has_function('backtrace')
    execinfo_dep = dependency('', required: false)
  else
    execinfo_dep = cc.find_library('execinfo')
  endif
  cdata.set('HAVE_BACKTRACE', 1)
else
  execinfo_dep = cc.find_library('execinfo', required: get_option('execinfo'))
  if cc.has_function('backtrace', required: get_option('execinfo')) or execinfo_dep.found()
    cdata.set('HAVE_BACKTRACE', 1)
  endif
endif

if cdata.has('USE_EPOLL')
  if get_option('kqueue').enabled()
    error('epoll and kqueue are mutually exclusive. Please enable only one')
  endif
elif get_option('kqueue').disabled()
  error('One of epoll or kqueue must be enabled')
endif

kqueue_opt = get_option('kqueue').disable_auto_if(cdata.has('USE_EPOLL'))
if kqueue_opt.allowed()
  if cc.has_header('sys/event.h', required: false)
    kqueue_dep = dependency('', required: false)
  else
    kqueue_dep = dependency('libkqueue')
  endif
  cdata.set('USE_KQUEUE', 1)
else
  kqueue_dep = dependency('', required: false)
endif

if cc.links('#include <atomic>\n#include <bitset>\nint main(){std::atomic<std::bitset<3>> b;std::atomic_is_lock_free(&b);}')
  atomic_dep = dependency('', required: false)
else
  atomic_dep = cc.find_library('atomic')
endif

if host_machine.system() != 'sunos'
  socket_dep = dependency('', required: false)
else
  socket_dep = cc.find_library('socket')
endif

curl_dep = dependency('libcurl')
ssl_dep = dependency('libcrypto')
thread_dep = dependency('threads')
zlib_dep = dependency('zlib')

cfile = configure_file(
  configuration: cdata,
  output: 'config.h',
)

subdir('src')
subdir('test')
