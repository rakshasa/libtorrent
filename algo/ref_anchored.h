#ifndef ALGO_REF_ANCHORED_H
#define ALGO_REF_ANCHORED_H

// These classes implement a generic way of reference counting an object
// that is shared from a common base. Once all the outstanding references
// are removed, the object is deleted and the anchor is cleared.

// The interface should be pretty self-explanatory.

namespace algo {

template <typename>
class RefAnchor;

template <typename>
class RefAnchored;

template <typename Type>
class RefAnchorData {
protected:
  template <class> friend class RefAnchored;
  template <class> friend class RefAnchor;

  RefAnchorData(Type* d) : m_ref(1), m_data(d), m_anchor(NULL) {}

  ~RefAnchorData() {
    delete m_data;

    if (m_anchor)
      m_anchor->clear();
  }

  int m_ref;
  Type* m_data;

  RefAnchor<Type>* m_anchor;
};

template <typename Type>
class RefAnchor {
public:
  template<typename> friend class RefAnchored;

  RefAnchor() : m_ptr(NULL)                   {}
  RefAnchor(const RefAnchor& a) : m_ptr(NULL) {}
  ~RefAnchor()                                { clear(); }

  bool  is_valid()           { return m_ptr && m_ptr->m_data; }

  void clear() {
    if (m_ptr)
      m_ptr->m_anchor = NULL;

    m_ptr = NULL;
  }

  Type* data() { return m_ptr ? m_ptr->m_data : NULL; }

  RefAnchor& operator = (const RefAnchor& a)  { clear(); return *this; }

protected:
  void set(RefAnchorData<Type>* d) {
    if (m_ptr)
      m_ptr->m_anchor = NULL;

    m_ptr = d;

    if (m_ptr) {
      if (m_ptr->m_anchor)
	m_ptr->m_anchor->set(NULL);

      m_ptr->m_anchor = this;
    }
  }

  RefAnchorData<Type>* m_ptr;
};

template <typename Type>
class RefAnchored {
public:
  RefAnchored()                     : m_ptr(NULL)                       {}
  RefAnchored(Type* t)              : m_ptr(new RefAnchorData<Type>(t)) {}
  RefAnchored(RefAnchor<Type>& r)   : m_ptr(r.m_ptr)                    { if (m_ptr) m_ptr->m_ref++; }
  RefAnchored(const RefAnchored& r) : m_ptr(r.m_ptr)                    { if (m_ptr) m_ptr->m_ref++; }
  ~RefAnchored()                                                        { clear(); }

  RefAnchored& operator = (const RefAnchored& a) {
    if (&a == this)
      return *this;

    clear();
    m_ptr = a.m_ptr;

    if (m_ptr) m_ptr->m_ref++;

    return *this;
  }

  bool  is_valid()                 { return m_ptr && m_ptr->m_data; }

  void  anchor(RefAnchor<Type>& a) { a.set(m_ptr); }
  void  clear()                    { if (m_ptr && --m_ptr->m_ref == 0) delete m_ptr; m_ptr = NULL; }

  // Clear the anchor so this object will die out. Use only if you know it
  // is safe.
  void  drift()                    { if (m_ptr && m_ptr->m_anchor) m_ptr->m_anchor->clear(); }

  Type* data()                     { return m_ptr ? m_ptr->m_data : NULL; }

  Type& operator * ()              { return *m_ptr->m_data; }
  Type* operator -> ()             { return m_ptr->m_data; }

private:
  RefAnchorData<Type>* m_ptr;
};

}

#endif

