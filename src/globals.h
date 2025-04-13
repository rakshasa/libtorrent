#ifndef LIBTORRENT_GLOBALS_H
#define LIBTORRENT_GLOBALS_H

#include "rak/timer.h"
#include "rak/priority_queue_default.h"
#include "torrent/common.h"

namespace torrent {

// TODO: Move to Thread.
LIBTORRENT_EXPORT extern rak::priority_queue_default taskScheduler;
LIBTORRENT_EXPORT extern rak::timer                  cachedTime;

}

#endif
