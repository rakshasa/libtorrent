// rTorrent - BitTorrent client
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

#ifndef ALGO_USE_IMPL_H
#define ALGO_USE_IMPL_H

#include "internal_impl.h"

namespace algo {

template <typename Type>
struct Value {
  Value(Type value) : m_value(value) {}

  Type& operator () () {
    return m_value;
  }

  template <typename Arg1>
  Type& operator () (Arg1& arg1) {
    return m_value;
  }

  template <typename Ret>
  Type& operator () () {
    return m_value;
  }

  template <typename Ret, typename Arg1>
  Type& operator () (Arg1& arg1) {
    return m_value;
  }

  Type m_value;
};

template <typename Type>
struct Ref {
  Ref(Type& value) : m_value(value) {}

  Type& operator () () {
    return m_value;
  }

  template <typename Arg1>
  Type& operator () (Arg1& arg1) {
    return m_value;
  }

  template <typename Ret>
  Type& operator () () {
    return m_value;
  }

  template <typename Ret, typename Arg1>
  Type& operator () (Arg1& arg1) {
    return m_value;
  }

  Type& m_value;
};

template <typename Target, typename Call>
struct On {
  On(Target target, Call call) : m_target(target), m_call(call) {}

  void operator () () {
    m_call(m_target());
  }

  template <typename Arg1>
  void operator () (Arg1& a1) {
    m_call(m_target(a1));
  }

  template <typename Ret>
  Ret operator () () {
    return m_call(m_target());
  }

  template <typename Ret, typename Arg1>
  Ret operator () (Arg1& a1) {
    return m_call(m_target(a1));
  }

  Target m_target;
  Call m_call;
};

template <typename Type>
struct Call {
  Call(Type (*f)()) : m_f(f) {}

  Type operator () () {
    return m_f();
  }

  template <typename Arg1>
  Type operator () (Arg1& a1) {
    return m_f();
  }

  template <typename Ret>
  Type operator () () {
    return m_f();
  }

  template <typename Ret, typename Arg1>
  Type operator () (Arg1& a1) {
    return m_f();
  }

  Type (*m_f)();
};

template <typename Type, typename Type1, typename P1>
struct Call1 {
  Call1(Type (*f)(Type1), P1 p1) :
    m_f(f), m_p1(p1) {}

  Type operator () () {
    return m_f(m_p1());
  }

  template <typename Arg1>
  Type operator () (Arg1& a1) {
    return m_f(m_p1(a1));
  }

  template <typename Ret>
  Type operator () () {
    return m_f(m_p1());
  }

  template <typename Ret, typename Arg1>
  Type operator () (Arg1& a1) {
    return m_f(m_p1(a1));
  }

  Type (*m_f)(Type1);
  P1 m_p1;
};

template <typename Type, typename Type1, typename Type2, typename P1, typename P2>
struct Call2 {
  Call2(Type (*f)(Type1, Type2), P1 p1, P2 p2) :
    m_f(f), m_p1(p1), m_p2(p2) {}

  Type operator () () {
    return m_f(m_p1(), m_p2());
  }

  template <typename Arg1>
  Type operator () (Arg1& a1) {
    return m_f(m_p1(a1), m_p2(a1));
  }

  template <typename Ret>
  Type operator () () {
    return m_f(m_p1(), m_p2());
  }

  template <typename Ret, typename Arg1>
  Type operator () (Arg1& a1) {
    return m_f(m_p1(a1), m_p2(a1));
  }

  Type (*m_f)(Type1, Type2);
  P1 m_p1;
  P2 m_p2;
};

template <typename Type, typename Type1, typename Type2, typename Type3,
	  typename P1, typename P2, typename P3>
struct Call3 {
  Call3(Type (*f)(Type1, Type2, Type3),
	P1 p1,
	P2 p2,
	P3 p3) :
    m_f(f), m_p1(p1), m_p2(p2), m_p3(p3) {}

  Type operator () () {
    return m_f(m_p1(), m_p2(), m_p3());
  }

  template <typename Arg1>
  Type operator () (Arg1& a1) {
    return m_f(m_p1(a1), m_p2(a1), m_p3(a1));
  }

  template <typename Ret>
  Type operator () () {
    return m_f(m_p1(), m_p2(), m_p3());
  }

  template <typename Ret, typename Arg1>
  Type operator () (Arg1& a1) {
    return m_f(m_p1(a1), m_p2(a1), m_p3(a1));
  }

  Type (*m_f)(Type1, Type2, Type3);
  P1 m_p1;
  P2 m_p2;
  P3 m_p3;
};

template <typename Class, typename Type, typename Target>
struct CallMember {
  typedef Type BaseType;

  CallMember(Target target, Type (Class::*f)()) :
    m_target(target), m_f(f) {}

  Type operator () () {
    return (Direct(m_target()).*m_f)();
  }

  template <typename Arg1>
  Type operator () (Arg1& a1) {
    return (Direct(m_target(a1)).*m_f)();
  }

  template <typename Ret>
  Type operator () () {
    return (Direct(m_target()).*m_f)();
  }

  template <typename Ret, typename Arg1>
  Type operator () (Arg1& a1) {
    return (Direct(m_target(a1)).*m_f)();
  }

  Target m_target;
  Type (Class::*m_f)();
};

template <typename Class, typename Type, typename Type1, typename Target, typename P1>
struct CallMember1 {
  typedef Type BaseType;

  CallMember1(Target target, Type (Class::*f)(Type1), P1 p1) :
    m_target(target), m_f(f), m_p1(p1) {}

  Type operator () () {
    return (Direct(m_target()).*m_f)(m_p1());
  }

  template <typename Arg1>
  Type operator () (Arg1& a1) {
    return (Direct(m_target(a1)).*m_f)(m_p1(a1));
  }

  template <typename Ret>
  Type operator () () {
    return (Direct(m_target()).*m_f)(m_p1());
  }

  template <typename Ret, typename Arg1>
  Type operator () (Arg1& a1) {
    return (Direct(m_target(a1)).*m_f)(m_p1(a1));
  }

  Target m_target;
  Type (Class::*m_f)(Type1);
  P1 m_p1;
};

template <typename Class, typename Type, typename Type1, typename Type2,
	  typename Target, typename P1, typename P2>
struct CallMember2 {
  typedef Type BaseType;

  CallMember2(Target target, Type (Class::*f)(Type1, Type2), P1 p1, P2 p2) :
    m_target(target), m_f(f), m_p1(p1), m_p2(p2) {}

  Type operator () () {
    return (Direct(m_target()).*m_f)(m_p1(), m_p2());
  }

  template <typename Arg1>
  Type operator () (Arg1& a1) {
    return (Direct(m_target(a1)).*m_f)(m_p1(a1), m_p2(a1));
  }

  template <typename Ret>
  Type operator () () {
    return (Direct(m_target()).*m_f)(m_p1(), m_p2());
  }

  template <typename Ret, typename Arg1>
  Type operator () (Arg1& a1) {
    return (Direct(m_target(a1)).*m_f)(m_p1(a1), m_p2(a1));
  }

  Target m_target;
  Type (Class::*m_f)(Type1, Type2);
  P1 m_p1;
  P2 m_p2;
};

template <typename Class, typename Type, typename Type1, typename Type2, typename Type3,
	  typename Target, typename P1, typename P2, typename P3>
struct CallMember3 {
  typedef Type BaseType;

  CallMember3(Target target, Type (Class::*f)(Type1, Type2, Type3), P1 p1, P2 p2, P3 p3) :
    m_target(target), m_f(f), m_p1(p1), m_p2(p2), m_p3(p3) {}

  Type operator () () {
    return (Direct(m_target()).*m_f)(m_p1(), m_p2(), m_p3());
  }

  template <typename Arg1>
  Type operator () (Arg1& a1) {
    return (Direct(m_target(a1)).*m_f)(m_p1(a1), m_p2(a1), m_p3(a1));
  }

  template <typename Ret>
  Type operator () () {
    return (Direct(m_target()).*m_f)(m_p1(), m_p2(), m_p3());
  }

  template <typename Ret, typename Arg1>
  Type operator () (Arg1& a1) {
    return (Direct(m_target(a1)).*m_f)(m_p1(a1), m_p2(a1), m_p3());
  }

  Target m_target;
  Type (Class::*m_f)(Type1, Type2, Type3);
  P1 m_p1;
  P2 m_p2;
  P3 m_p3;
};

template <typename Class, typename Type>
struct MemberVar {
  typedef Type BaseType;

  MemberVar(Type Class::*v) : m_v(v) {}

  template <typename Arg1>
  Type& operator () (Arg1& arg1) {
    return Direct(arg1).*m_v;
  }

  template <typename Ret, typename Arg1>
  Type& operator () (Arg1& arg1) {
    return Direct(arg1).*m_v;
  }

  Type Class::*m_v;
};

template <typename Class, typename P1, typename P2>
struct CallCtor2 {
  CallCtor2(P1 p1, P2 p2) : m_p1(p1), m_p2(p2) {}

  Class operator () () {
    return Class(m_p1(), m_p2());
  }

  template <typename Arg1>
  Class operator () (Arg1& a1) {
    return Class(m_p1(a1), m_p2(a1));
  }

  template <typename Ret>
  Class operator () () {
    return Class(m_p1(), m_p2());
  }

  template <typename Ret, typename Arg1>
  Class operator () (Arg1& a1) {
    return Class(m_p1(a1), m_p2(a1));
  }
  
  P1 m_p1;
  P2 m_p2;
};

}

#endif // ALGO_USE_IMPL_H
