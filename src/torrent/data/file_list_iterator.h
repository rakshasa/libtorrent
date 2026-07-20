#ifndef LIBTORRENT_FILE_LIST_ITERATOR_H
#define LIBTORRENT_FILE_LIST_ITERATOR_H

#include <cstdlib>
#include <torrent/common.h>
#include <torrent/data/file_list.h>

namespace torrent {

class File;

// A special purpose iterator class for iterating through FileList as
// a dired structure.
class LIBTORRENT_EXPORT FileListIterator {
public:
  using iterator  = FileList::iterator;
  using reference = File*;
  using pointer   = File**;

  FileListIterator() = default;
  explicit FileListIterator(iterator pos, uint32_t depth = 0) : m_position(pos), m_depth(depth) {}

  bool                is_file() const;
  bool                is_empty() const;

  bool                is_entering() const;
  bool                is_leaving() const  { return m_depth < 0; }

  uint32_t            depth() const       { return std::abs(m_depth); }

  iterator            base() const        { return m_position; }
  reference           file() const        { return m_position->get(); }

  FileListIterator&   operator ++();
  FileListIterator&   operator --();

  FileListIterator    operator ++(int);
  FileListIterator    operator --(int);

  FileListIterator&   forward_current_depth();
  FileListIterator&   backward_current_depth();

  friend bool         operator ==(const FileListIterator& left, const FileListIterator& right);
  friend bool         operator !=(const FileListIterator& left, const FileListIterator& right);

private:
  iterator            m_position;
  int32_t             m_depth;
};

inline FileListIterator
FileListIterator::operator ++(int) {
  FileListIterator tmp = *this;
  ++(*this);
  return tmp;
}

inline FileListIterator
FileListIterator::operator --(int) {
  FileListIterator tmp = *this;
  --(*this);
  return tmp;
}

inline bool
operator ==(const FileListIterator& left, const FileListIterator& right) {
  return left.m_position == right.m_position && left.m_depth == right.m_depth;
}

inline bool
operator !=(const FileListIterator& left, const FileListIterator& right) {
  return left.m_position != right.m_position || left.m_depth != right.m_depth;
}

// Take a range as input and return the next entry at the same
// directory depth as first. If the returned iterator equals 'last' or
// is_leaving() == true then the search failed.
class LIBTORRENT_EXPORT file_list_collapsed_iterator : private FileListIterator {
public:
  using base_type = FileListIterator;
  using this_type = file_list_collapsed_iterator;

  using base_type::iterator;
  using base_type::reference;
  using base_type::pointer;

  using base_type::is_file;
  using base_type::is_empty;
  using base_type::is_entering;
  using base_type::is_leaving;
  using base_type::depth;
  using base_type::file;

  file_list_collapsed_iterator() = default;
  file_list_collapsed_iterator(const FileListIterator& src) : FileListIterator(src) {}
  explicit file_list_collapsed_iterator(iterator pos, uint32_t depth = 0) : FileListIterator(pos, depth) {}

  base_type           base() const { return *static_cast<const base_type*>(this); }

  this_type&          operator ++() { base_type::forward_current_depth(); return *this; }
  this_type&          operator --() { base_type::backward_current_depth(); return *this; }

  this_type           operator ++(int);
  this_type           operator --(int);

  friend bool         operator ==(const file_list_collapsed_iterator& left, const file_list_collapsed_iterator& right);
  friend bool         operator !=(const file_list_collapsed_iterator& left, const file_list_collapsed_iterator& right);
};

inline bool
operator ==(const file_list_collapsed_iterator& left, const file_list_collapsed_iterator& right) {
  return left.base() == right.base();
}

inline bool
operator !=(const file_list_collapsed_iterator& left, const file_list_collapsed_iterator& right) {
  return left.base() != right.base();
}

inline file_list_collapsed_iterator
file_list_collapsed_iterator::operator ++(int) {
  this_type tmp = *this;
  ++(*this);
  return tmp;
}

inline file_list_collapsed_iterator
file_list_collapsed_iterator::operator --(int) {
  this_type tmp = *this;
  --(*this);
  return tmp;
}

} // namespace torrent

#endif
