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
// Contact:  Jari Sundell <jaris@ifi.uio.no>
//
//           Skomakerveien 33
//           3185 Skoppum, NORWAY

#ifndef LIBTORRENT_UTILS_THROTTLE_CONTROL_H
#define LIBTORRENT_UTILS_THROTTLE_CONTROL_H

#include "rate.h"
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
  
  Rate&               get_rate()                    { return m_rate; }

  void                start()                       { m_taskTick.insert(Timer::cache()); }
  void                stop()                        { m_taskTick.remove(); }

private:
  ThrottleControl(const ThrottleControl&);
  void operator = (const ThrottleControl&);

  void                receive_tick();

  int                 m_interval;
  int                 m_quota;

  Task                m_taskTick;
  Rate                m_rate;
};

template <typename T>
ThrottleControl<T>::ThrottleControl() :
  m_interval(1000000),
  m_quota(UNLIMITED),
  m_taskTick(sigc::mem_fun(*this, &ThrottleControl::receive_tick)) {
}

template <typename T> void
ThrottleControl<T>::receive_tick() {
  if (!is_unlimited())
    Base::quota(m_quota + std::max<int>(0, m_quota - m_rate.rate()) * 30);
  else
    Base::quota(m_quota);

  m_taskTick.insert(Timer::cache() + m_interval);
}

}

#endif
