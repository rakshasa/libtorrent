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
  Convert(Ftor ftor) : m_ftor(ftor) {}

  Ret operator () () {
    return m_ftor.operator()<Ret>();
  }

  template <typename Arg1>
  Ret operator () (Arg1& a1) {
    return m_ftor.operator()<Ret>(a1);
  }

  template <typename RetIgnore, typename Arg1>
  Ret operator () (Arg1& a1) {
    return m_ftor.operator()<Ret>(a1);
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
