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

#ifndef ALGO_COMMON_IMPL_H
#define ALGO_COMMON_IMPL_H

namespace algo {

struct empty {
  void operator () () {
  }

  template <typename Ret>
  void operator () () {
  }

  template <typename Arg1>
  void operator () (Arg1& a1) {
  }

  template <typename Ret, typename Arg1>
  void operator () (Arg1& a1) {
  }
};

struct back_as_value {
  template <typename Arg1>
  Arg1 operator () (Arg1& a1) {
    return a1;
  }

  template <typename Ret, typename Arg1>
  Arg1 operator () (Arg1& a1) {
    return a1;
  }
};

struct back_as_ref {
  template <typename Arg1>
  Arg1& operator () (Arg1& a1) {
    return a1;
  }

  template <typename Arg1>
  Arg1& operator () (Arg1* a1) {
    return *a1;
  }

  template <typename Ret, typename Arg1>
  Arg1& operator () (Arg1& a1) {
    return a1;
  }

  template <typename Ret, typename Arg1>
  Arg1& operator () (Arg1* a1) {
    return *a1;
  }
};

template<typename Type>
struct back_as_ref_t {
  typedef Type BaseType;

  template <typename Arg1>
  BaseType& operator () (Arg1& a1) {
    return a1;
  }

  template <typename Arg1>
  BaseType& operator () (Arg1* a1) {
    return *a1;
  }

  template <typename Ret, typename Arg1>
  BaseType& operator () (Arg1& a1) {
    return a1;
  }

  template <typename Ret, typename Arg1>
  BaseType& operator () (Arg1* a1) {
    return *a1;
  }
};

struct back_as_ptr {
  template <typename Arg1>
  Arg1* operator () (Arg1& a1) {
    return &a1;
  }

  template <typename Arg1>
  Arg1* operator () (Arg1* a1) {
    return a1;
  }

  template <typename Ret, typename Arg1>
  Arg1* operator () (Arg1& a1) {
    return &a1;
  }

  template <typename Ret, typename Arg1>
  Arg1* operator () (Arg1* a1) {
    return a1;
  }
};

template <typename Ret, typename Ftor>
struct Convert {
  typedef Ret BaseType;

  Convert(Ftor ftor) : m_ftor(ftor) {}

  Ret operator () () {
    return m_ftor.operator()<Ret>();
  }

  template <typename Arg1>
  Ret operator () (Arg1& a1) {
    return m_ftor.operator()<Ret, Arg1>(a1);
  }

  template <typename RetIgnore, typename Arg1>
  Ret operator () (Arg1& a1) {
    return m_ftor.operator()<Ret, Arg1>(a1);
  }

  Ftor m_ftor;
};

template <typename Ret, typename Ftor>
struct ConvertPtr {
  ConvertPtr(Ftor ftor) : m_ftor(ftor) {}

  Ret* operator () () {
    return (Ret*)&m_ftor.operator()<Ret>();
  }

  template <typename Arg1>
  Ret* operator () (Arg1& a1) {
    return (Ret*)&m_ftor.operator()<Ret>(a1);
  }

  template <typename RetIgnore, typename Arg1>
  Ret* operator () (Arg1& a1) {
    return (Ret*)&m_ftor.operator()<Ret>(a1);
  }

  Ftor m_ftor;
};

struct NewOn {
  template <class T>
  void operator () (T*& t) {
    t = new T();
  }
};

struct DeleteOn {
  template <class T>
  void operator () (T* t) {
    delete t;
  }
};

template <typename Cond, typename Then, typename Else>
struct IfOn {
  IfOn(Cond c, Then t, Else e) : m_c(c), m_t(t), m_e(e) {}

  void operator () () {
    if (m_c())
      m_t();
    else
      m_e();
  }

  template <typename Arg1>
  void operator () (Arg1& a1) {
    if (m_c(a1))
      m_t(a1);
    else
      m_e(a1);
  }

  template <typename Ret>
  Ret operator () () {
    if (m_c())
      return m_t.operator()<Ret>();
    else
      return m_e.operator()<Ret>();
  }

  template <typename Ret, typename Arg1>
  Ret operator () (Arg1& a1) {
    if (m_c(a1))
      return m_t.operator()<Ret>(a1);
    else
      return m_e.operator()<Ret>(a1);
  }

  Cond m_c;
  Then m_t;
  Else m_e;
};

template <typename Ftor1, typename Ftor2>
struct Branch {
  Branch(Ftor1 ftor1, Ftor2 ftor2) : m_ftor1(ftor1), m_ftor2(ftor2) {}

  void operator () () {
    m_ftor1();
    m_ftor2();
  }

  template <typename Arg1>
  void operator () (Arg1& a1) {
    m_ftor1(a1);
    m_ftor2(a1);
  }

  template <typename Ret, typename Arg1>
  Ret operator () (Arg1& a1) {
    m_ftor1(a1);
    return m_ftor2(a1);
  }

  Ftor1 m_ftor1;
  Ftor2 m_ftor2;
};

}

#endif // ALGO_COMMON_IMPL_H
