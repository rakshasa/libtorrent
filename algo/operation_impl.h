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

#ifndef ALGO_OPERATION_IMPL_H
#define ALGO_OPERATION_IMPL_H

#define ALGO_OPERATION_CODE_ARIT( NAME1, NAME2, OP )			\
template <typename Ftor1, typename Ftor2>				\
struct NAME1 {								\
  NAME1(Ftor1 ftor1, Ftor2 ftor2) : m_ftor1(ftor1), m_ftor2(ftor2) {}	\
									\
  template <typename Ret>						\
  Ret operator () () {							\
    return m_ftor1.operator()<Ret>() OP m_ftor2.operator()<Ret>();	\
  }									\
									\
  template <typename Ret, typename Arg1>				\
  Ret operator () (Arg1& a1) {						\
    return m_ftor1.operator()<Ret>(a1) OP m_ftor2.operator()<Ret>(a1);	\
  }									\
									\
  Ftor1 m_ftor1;							\
  Ftor2 m_ftor2;							\
};									\
									\
template <typename Ftor1, typename Ftor2>				\
inline NAME1<Ftor1, Ftor2>						\
NAME2(Ftor1 ftor1, Ftor2 ftor2) {					\
  return NAME1<Ftor1, Ftor2>(ftor1, ftor2);				\
}									\
									\
template <typename Ret, typename Ftor1, typename Ftor2>			\
inline Convert<Ret, NAME1<Ftor1, Ftor2> >				\
NAME2(Ftor1 ftor1, Ftor2 ftor2) {					\
  return convert<Ret>(NAME1<Ftor1, Ftor2>(ftor1, ftor2));		\
}

#define ALGO_OPERATION_CODE_ARTO( NAME1, NAME2, NAME3, OP )		\
template <typename Target, typename Call>				\
struct NAME1 {								\
  NAME1(Target target, Call call) : m_target(target), m_call(call) {}	\
									\
  void operator () () {							\
    m_target() OP##= m_call();						\
  }									\
									\
  template <typename Arg1>						\
  void operator () (Arg1& t) {						\
    m_target(t) OP##= m_call(t);					\
  }									\
									\
  Target m_target;							\
  Call m_call;								\
};									\
									\
template <typename Target, typename Call>				\
inline NAME1<Target, Call>						\
NAME2(Target target, Call call) {					\
  return NAME1<Target, Call>(target, call);				\
}									\
									\
template <typename Target, typename Call>				\
inline NAME1<Ref<Target>, Convert<Target, Call> >			\
NAME3(Target& target, Call call) {					\
  return NAME2(ref(target), convert<Target>(call));			\
}

// TODO: Can this be done using convert instead?
#define ALGO_OPERATION_CODE_BOOL( NAME1, NAME2, OP )			\
									\
template <typename Ftor1, typename Ftor2>				\
struct NAME1 {								\
  NAME1(Ftor1 ftor1, Ftor2 ftor2) : m_ftor1(ftor1), m_ftor2(ftor2) {}	\
									\
  bool operator () () {							\
    return m_ftor1() OP m_ftor2();					\
  }									\
									\
  template <typename Arg1>						\
  bool operator () (Arg1& a1) {						\
    return m_ftor1(a1) OP m_ftor2(a1);					\
  }									\
									\
  template <typename Ret>						\
  bool operator () () {							\
    return m_ftor1() OP m_ftor2();					\
  }									\
									\
  template <typename Ret, typename Arg1>				\
  bool operator () (Arg1& a1) {						\
    return m_ftor1(a1) OP m_ftor2(a1);					\
  }									\
									\
  template <typename Arg1, typename Arg2>				\
  bool operator () (Arg1& a1, Arg2& a2) {				\
    return m_ftor1(a1) OP m_ftor2(a2);					\
  }									\
									\
  Ftor1 m_ftor1;							\
  Ftor2 m_ftor2;							\
};									\
									\
template <typename Ftor1, typename Ftor2>				\
inline NAME1<Ftor1, Ftor2> NAME2(Ftor1 ftor1, Ftor2 ftor2) {		\
  return NAME1<Ftor1, Ftor2>(ftor1, ftor2);				\
}									\
									\
template <typename Class, typename Type, typename Ftor2>		\
inline NAME1<MemberVar<Class, Type>, Ftor2>				\
NAME2(Type Class::*m, Ftor2 ftor2) {					\
  return NAME1<MemberVar<Class, Type>, Ftor2>(member(m), ftor2);	\
}									\
									\
template <typename Ftor1>						\
inline NAME1<Ftor1, back_as_ref> NAME2(Ftor1 ftor1) {			\
  return NAME1<Ftor1, back_as_ref>(ftor1, back_as_ref());		\
}

#define ALGO_OPERATION_CODE_BOOL_UNI( NAME1, NAME2, OP )	\
template <typename Ftor1>					\
struct NAME1 {							\
  NAME1(Ftor1 ftor1) : m_ftor1(ftor1) {}			\
								\
  bool operator () () {						\
    return OP m_ftor1();					\
  }								\
								\
  template <typename Arg1>					\
  bool operator () (Arg1& a1) {					\
    return OP m_ftor1(a1);					\
  }								\
								\
  template <typename Ret>					\
  bool operator () () {						\
    return OP m_ftor1();					\
  }								\
								\
  template <typename Ret, typename Arg1>			\
  bool operator () (Arg1& a1) {					\
    return OP m_ftor1(a1);					\
  }								\
								\
  Ftor1 m_ftor1;						\
};								\
								\
template <typename Ftor1>					\
inline NAME1<Ftor1>						\
NAME2(Ftor1 ftor1) {						\
  return NAME1<Ftor1>(ftor1);					\
}

#endif // ALGO_OPERATION_IMPL_H
