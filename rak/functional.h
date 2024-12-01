// rak - Rakshasa's toolbox
// Copyright (C) 2005-2007, Jari Sundell
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

#ifndef RAK_FUNCTIONAL_H
#define RAK_FUNCTIONAL_H

#include <cstddef>
#include <functional>

namespace rak {

template <typename Type>
struct reference_fix {
  typedef Type type;
};

template <typename Type>
struct reference_fix<Type&> {
  typedef Type type;
};

// Operators:

template <typename Type, typename Ftor>
struct equal_t {
  typedef bool result_type;

  equal_t(Type t, Ftor f) : m_t(t), m_f(f) {}

  template <typename Arg>
  bool operator () (Arg& a) {
    return m_t == m_f(a);
  }

  Type m_t;
  Ftor m_f;
};

template <typename Type, typename Ftor>
inline equal_t<Type, Ftor>
equal(Type t, Ftor f) {
  return equal_t<Type, Ftor>(t, f);
}

template <typename Type, typename Ftor>
struct less_t {
  typedef bool result_type;

  less_t(Type t, Ftor f) : m_t(t), m_f(f) {}

  template <typename Arg>
  bool operator () (Arg& a) {
    return m_t < m_f(a);
  }

  Type m_t;
  Ftor m_f;
};

template <typename Type, typename Ftor>
inline less_t<Type, Ftor>
less(Type t, Ftor f) {
  return less_t<Type, Ftor>(t, f);
}

template <typename Type, typename Ftor>
struct less_equal_t {
  typedef bool result_type;

  less_equal_t(Type t, Ftor f) : m_t(t), m_f(f) {}

  template <typename Arg>
  bool operator () (Arg& a) {
    return m_t <= m_f(a);
  }

  Type m_t;
  Ftor m_f;
};

template <typename Type, typename Ftor>
inline less_equal_t<Type, Ftor>
less_equal(Type t, Ftor f) {
  return less_equal_t<Type, Ftor>(t, f);
}

template <typename Src, typename Dest>
struct on_t : public std::unary_function<typename Src::argument_type, typename Dest::result_type> {
  typedef typename Dest::result_type result_type;

  on_t(Src s, Dest d) : m_dest(d), m_src(s) {}

  result_type operator () (typename reference_fix<typename Src::argument_type>::type arg) {
    return m_dest(m_src(arg));
  }

  Dest m_dest;
  Src m_src;
};
    
template <typename Src, typename Dest>
inline on_t<Src, Dest>
on(Src s, Dest d) {
  return on_t<Src, Dest>(s, d);
}  

template <typename Class, typename Member>
struct mem_ref_t : public std::unary_function<Class&, Member&> {
  mem_ref_t(Member Class::*m) : m_member(m) {}

  Member& operator () (Class& c) {
    return c.*m_member;
  }

  Member Class::*m_member;
};

template <typename Class, typename Member>
struct const_mem_ref_t : public std::unary_function<const Class&, const Member&> {
  const_mem_ref_t(const Member Class::*m) : m_member(m) {}

  const Member& operator () (const Class& c) {
    return c.*m_member;
  }

  const Member Class::*m_member;
};

template <typename Class, typename Member>
inline mem_ref_t<Class, Member>
mem_ref(Member Class::*m) {
  return mem_ref_t<Class, Member>(m);
}

template <typename Class, typename Member>
inline const_mem_ref_t<Class, Member>
const_mem_ref(const Member Class::*m) {
  return const_mem_ref_t<Class, Member>(m);
}

template <typename T>
struct call_delete : public std::unary_function<T*, void> {
  void operator () (T* t) {
    delete t;
  }
};

// Lightweight callback function including pointer to object. Should
// be replaced by TR1 stuff later. Requires an object to bind, instead
// of using a seperate functor for that.

template <typename Object, typename Ret, typename Arg1>
class mem_fun1 {
public:
  typedef Ret result_type;
  typedef Ret (Object::*Function)(Arg1);

  mem_fun1() : m_object(NULL) {}
  mem_fun1(Object* o, Function f) : m_object(o), m_function(f) {}

  bool is_valid() const { return m_object; }

  Ret operator () (Arg1 a1) { return (m_object->*m_function)(a1); }
  
private:
  Object* m_object;
  Function m_function;
};

template <typename Object, typename Ret, typename Arg1>
inline mem_fun1<Object, Ret, Arg1>
make_mem_fun(Object* o, Ret (Object::*f)(Arg1)) {
 return mem_fun1<Object, Ret, Arg1>(o, f);
}

template <typename Container>
inline void
slot_list_call(const Container& slot_list) {
  if (slot_list.empty())
    return;

  typename Container::const_iterator first = slot_list.begin();
  typename Container::const_iterator next = slot_list.begin();

  while (++next != slot_list.end()) {
    (*first)();
    first = next;
  }

  (*first)();
}

template <typename Container, typename Arg1>
inline void
slot_list_call(const Container& slot_list, Arg1 arg1) {
  if (slot_list.empty())
    return;

  typename Container::const_iterator first = slot_list.begin();
  typename Container::const_iterator next = slot_list.begin();

  while (++next != slot_list.end()) {
    (*first)(arg1);
    first = next;
  }

  (*first)(arg1);
}

}

#endif
