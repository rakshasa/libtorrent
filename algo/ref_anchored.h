#ifndef ALGO_REF_ANCHORED_H
#define ALGO_REF_ANCHORED_H

namespace algo {

template <typename Type>
class RefAnchoredData {
protected:
  friend class RefAnchored;
  friend class RefAnchor;

  RefAnchoredData(Type* d) : m_ref(1), m_data(d), m_anchor(NULL) {}

  ~RefAnchoredData() {
    if (m_anchor)
      m_anchor->clear();
  }

  int m_ref;
  Type* m_data;

  RefAnchor* m_anchor;
};

template <typename Type>
class RefAnchor {
public:
  friend class RefAnchored;

  RefAnchor() : m_data(NULL)                   {}
  RefAnchor(const RefAnchor& a) : m_data(NULL) {}
  ~RefAnchor()                                 { clear(); }

  RefAnchor& operator = (const RefAnchor& a)   { clear(); }

  void clear() {
    if (m_data == NULL)
      return;

    m_data->m_anchor = NULL;
    m_data = NULL;
  }

protected:
  void set(RefAnchorData& d) {
    clear();

    m_data = &d;
    m_data->m_anchor = this;
  }

private:
  RefAnchoredData* m_data;
};

template <typename Type>
class Reference {
public:
  RefAnchored()                     : m_ref(NULL)                 {}
  RefAnchored(Type* t)              : m_ref(new RefAnchorData(t)) {}
  RefAnchored(const RefAnchored& r) : m_ptr(r.m_ptr)              { if (m_ptr) m_ptr->m_ref++; }
  ~RefAnchored()                                                  { clear(); }

  RefAnchored& operator = (const RefAnchored& a) {
    clear();

    m_ref = a.m_ref;

    if (m_ptr) m_ptr->m_ref++;
  }

  Type* data()               { return m_ptr ? m_ptr->m_data : NULL; }

  void  anchor(RefAnchor& a) { a.set(m_ptr); }

  void  clear()              { if (m_ref && --m_ptr->m_ref == 0) delete m_ptr; m_ptr = NULL }

private:
  RefAnchorData* m_ptr;
};

}

#endif

