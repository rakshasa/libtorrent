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


#ifndef ALGO_USE_H
#define ALGO_USE_H

#include <algo/use_impl.h>

namespace algo {

template <typename Type>
inline Value<Type> value(Type v) {
  return Value<Type>(v);
}

template <typename Type>
inline Ref<Type> ref(Type& v) {
  return Ref<Type>(v);
}

// Target and Call do not inherit On's Ret template argument.
template <typename Target, typename Call>
inline On<Target, Call> on(Target target, Call call) {
  return On<Target, Call>(target, call);
}

// Target and Call do not inherit On's Ret template argument.
template <typename Target, typename Call, typename Ret>
inline Convert<Ret, On<Target, Call> > on(Target target, Call call) {
  return convert(On<Target, Call>(target, call));
}

template <typename Ret, typename Target, typename Call>
inline Convert<Ret, On<Target, Call> > on(Target target, Call call) {
  return convert<Ret>(On<Target, Call>(target, call));
}

template <typename Type>
inline Call<Type>
call(Type (*f)()) {
  return Call<Type>(f);
}

template <typename Type, typename Type1, typename P1>
inline Call1<Type, Type1, P1>
call(Type (*f)(Type1), P1 p1) {
  return Call1<Type, Type1, P1>(f, p1);
}

template <typename Type, typename Type1, typename Type2, typename P1, typename P2>
inline Call2<Type, Type1, Type2, P1, P2>
call(Type (*f)(Type1, Type2), P1 p1, P2 p2) {
  return Call2<Type, Type1, Type2, P1, P2>(f, p1, p2);
}

template <typename Type, typename Type1, typename Type2, typename Type3,
	  typename P1, typename P2, typename P3>
inline Call3<Type, Type1, Type2, Type3, P1, P2, P3>
call(Type (*f)(Type1, Type2, Type3), P1 p1, P2 p2, P3 p3) {
  return Call3<Type, Type1, Type2, Type3, P1, P2, P3>(f, p1, p2, p3);
}

template <typename Class, typename Type>
inline MemberVar<Class, Type>
member(Type Class::*v) {
  return MemberVar<Class, Type>(v);
}

// Same as:
// algo::on<*>(algo::member(&A::b), algo::member(&A::D::print1))

template <typename Class1, typename Type1, typename Class2, typename Type2>
inline Convert<Type2, On<MemberVar<Class1, Type1>, MemberVar<Class2, Type2> > >
member(Type1 Class1::*v1, Type2 Class2::*v2) {
  return on<Type2>(member(v1), member(v2));
}

// Do i want to leave 'member' capable calling functions?

template <typename Class, typename Type, typename Target>
inline CallMember<Class, Type, Target>
call_member(Target target, Type (Class::*f)()) {
  return CallMember<Class, Type, Target>(target, f);
}

template <typename Class, typename Type>
inline CallMember<Class, Type, back_as_ref>
call_member(Type (Class::*f)()) {
  return CallMember<Class, Type, back_as_ref>(back_as_ref(), f);
}

template <typename Class, typename Type, typename Type1, typename Target, typename P1>
inline CallMember1<Class, Type, Type1, Target, P1>
call_member(Target target, Type (Class::*f)(Type1), P1 p1) {
  return CallMember1<Class, Type, Type1, Target, P1>(target, f, p1);
}

template <typename Class, typename Type, typename Type1, typename P1>
inline CallMember1<Class, Type, Type1, back_as_ref, P1>
call_member(Type (Class::*f)(Type1), P1 p1) {
  return CallMember1<Class, Type, Type1, back_as_ref, P1>(back_as_ref(), f, p1);
}

template <typename Class, typename Type, typename Type1, typename Type2,
	  typename Target, typename P1, typename P2>
inline CallMember2<Class, Type, Type1, Type2, Target, P1, P2>
call_member(Target target, Type (Class::*f)(Type1, Type2), P1 p1, P2 p2) {
  return CallMember2<Class, Type, Type1, Type2, Target, P1, P2>(target, f, p1, p2);
}

template <typename Class, typename Type, typename Type1, typename Type2,
	  typename P1, typename P2>
inline CallMember2<Class, Type, Type1, Type2, back_as_ref, P1, P2>
call_member(Type (Class::*f)(Type1, Type2), P1 p1, P2 p2) {
  return CallMember2<Class, Type, Type1, Type2, back_as_ref, P1, P2>(back_as_ref(), f, p1, p2);
}

template <typename Class, typename Type, typename Type1, typename Type2, typename Type3,
	  typename Target, typename P1, typename P2, typename P3>
inline CallMember3<Class, Type, Type1, Type2, Type3, Target, P1, P2, P3>
call_member(Target target, Type (Class::*f)(Type1, Type2, Type3), P1 p1, P2 p2, P3 p3) {
  return CallMember3<Class, Type, Type1, Type2, Type3, Target, P1, P2, P3>(target, f, p1, p2, p3);
}

template <typename Class, typename Type, typename Type1, typename Type2, typename Type3,
	  typename P1, typename P2, typename P3>
inline CallMember3<Class, Type, Type1, Type2, Type3, back_as_ref, P1, P2, P3>
call_member(Type (Class::*f)(Type1, Type2, Type3), P1 p1, P2 p2, P3 p3) {
  return CallMember3<Class, Type, Type1, Type2, Type3, back_as_ref, P1, P2, P3>(back_as_ref(), f, p1, p2, p3);
}

template <typename Class, typename P1, typename P2>
inline CallCtor2<Class, P1, P2>
call_ctor(P1 p1, P2 p2) {
  return CallCtor2<Class, P1, P2>(p1, p2);
}

}

#endif // ALGO_USE_H
