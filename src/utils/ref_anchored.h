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

#ifndef LIBTORRENT_UTILS_REF_ANCHORED_H
#define LIBTORRENT_UTILS_REF_ANCHORED_H

// These classes implement a generic way of reference counting an object
// that is shared from a common base. Once all the outstanding references
// are removed, the object is deleted and the anchor is cleared.

// The interface should be pretty self-explanatory.

namespace torrent {

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

  bool  is_valid() const           { return m_ptr && m_ptr->m_data; }

  void  anchor(RefAnchor<Type>& a) { a.set(m_ptr); }
  void  clear()                    { if (m_ptr && --m_ptr->m_ref == 0) delete m_ptr; m_ptr = NULL; }

  // Clear the anchor so this object will die out. Use only if you know it
  // is safe.
  void  drift()                    { if (m_ptr && m_ptr->m_anchor) m_ptr->m_anchor->clear(); }

  Type* data()                     { return m_ptr ? m_ptr->m_data : NULL; }
  const Type* data() const         { return m_ptr ? m_ptr->m_data : NULL; }

  Type& operator * ()              { return *m_ptr->m_data; }
  const Type& operator * () const  { return *m_ptr->m_data; }
  Type* operator -> ()             { return m_ptr->m_data; }
  const Type* operator -> () const { return m_ptr->m_data; }

private:
  RefAnchorData<Type>* m_ptr;
};

}

#endif

