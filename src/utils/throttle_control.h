// libTorrent - BitTorrent library
// Copyright (C) 2005, Jari Sundell
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
// In addition, as a special exception, the copyright holders give
// permission to link the code of portions of this program with the
// OpenSSL library under certain conditions as described in each
// individual source file, and distribute linked combinations
// including the two.
//
// You must obey the GNU General Public License in all respects for
// all of the code used other than OpenSSL.  If you modify file(s)
// with this exception, you may extend this exception to your version
// of the file(s), but you are not obligated to do so.  If you do not
// wish to do so, delete this exception statement from your version.
// If you delete this exception statement from all source files in the
// program, then also delete it here.
//
// Contact:  Jari Sundell <jaris@ifi.uio.no>
//
//           Skomakerveien 33
//           3185 Skoppum, NORWAY

#ifndef LIBTORRENT_UTILS_THROTTLE_CONTROL_H
#define LIBTORRENT_UTILS_THROTTLE_CONTROL_H

#include "torrent/rate.h"
#include "task.h"
#include "throttle_list.h"

namespace torrent {

template <typename T>
class ThrottleControl : private ThrottleList<T> {
public:
  enum {
    UNLIMITED = -1
  };

  typedef ThrottleList<T> Base;

  typedef typename Base::iterator         iterator;
  typedef typename Base::reverse_iterator reverse_iterator;
  typedef typename Base::value_type       value_type;
  typedef typename Base::reference        reference;
  typedef typename Base::const_reference  const_reference;
  typedef typename Base::size_type        size_type;

  using Base::clear;
  using Base::size;
  using Base::insert;
  using Base::erase;

  using Base::begin;
  using Base::end;
  using Base::rbegin;
  using Base::rend;

  ThrottleControl();

  bool                is_unlimited() const          { return m_quota == UNLIMITED; }

  int                 get_interval() const          { return m_interval; }
  void                set_interval(int v)           { m_interval = v; }

  int                 get_quota() const             { return m_quota; }
  void                set_quota(int v)              { m_quota = v; }
  
  Rate&               get_rate_slow()               { return m_rateSlow; }
  Rate&               get_rate_quick()              { return m_rateQuick; }

  void                start()                       { taskScheduler.insert(&m_taskTick, Timer::cache()); }
  void                stop()                        { taskScheduler.erase(&m_taskTick); }

private:
  ThrottleControl(const ThrottleControl&);
  void operator = (const ThrottleControl&);

  void                receive_tick();

  int                 m_interval;
  int                 m_quota;

  TaskItem            m_taskTick;
  Rate                m_rateSlow;
  Rate                m_rateQuick;
};

template <typename T>
ThrottleControl<T>::ThrottleControl() :
  m_interval(1000000),
  m_quota(UNLIMITED),
  m_rateSlow(60),
  m_rateQuick(5) {

  m_taskTick.set_iterator(taskScheduler.end());
  m_taskTick.set_slot(sigc::mem_fun(*this, &ThrottleControl::receive_tick));
}

template <typename T> void
ThrottleControl<T>::receive_tick() {
  if (!is_unlimited()) {

    // We need to over-adjust to get closer to the desired rate. The
    // problem that we risk getting spikes of activity. Maybe we can
    // estimate an ideal quota that we can use, that will give us the
    // desired rate?
    int quota = (int)((m_quota + std::min(m_quota * 5, std::max<int>(0, m_quota - m_rateQuick.rate()) * 20))
		      * (m_interval / 1000000.0f));

    Base::quota(quota);
    //Base::quota(m_quota + std::min(m_quota, std::max<int>(0, m_quota - m_rateQuick.rate()) * 20));

  } else {
    Base::quota(m_quota);
  }

  taskScheduler.insert(&m_taskTick, Timer::cache() + m_interval);
}

}

#endif
