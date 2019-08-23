#ifndef LIBTORRENT_GLOBALS_H
#define LIBTORRENT_GLOBALS_H

#include <rak/timer.h>
#include <rak/priority_queue_default.h>

namespace torrent {

extern rak::priority_queue_default taskScheduler;
extern rak::timer                  cachedTime;

}

#endif
