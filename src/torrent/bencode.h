#ifndef LIBTORRENT_BENCODE_H
#define LIBTORRENT_BENCODE_H

#include <iosfwd>
#include <string>
#include <map>
#include <list>

namespace torrent {

// This class should very rarely change, so it doesn't matter that much
// of the implementation is visible.

class Bencode {
 public:
  typedef std::list<Bencode>             List;
  typedef std::map<std::string, Bencode> Map;

  enum Type {
    TYPE_NONE,
    TYPE_VALUE,
    TYPE_STRING,
    TYPE_LIST,
    TYPE_MAP
  };

  Bencode()                     : m_type(TYPE_NONE) {}
  Bencode(const int64_t v)      : m_type(TYPE_VALUE), m_value(v) {}
  Bencode(const std::string& s) : m_type(TYPE_STRING), m_string(new std::string(s)) {}
  Bencode(const Bencode& b);

  explicit Bencode(Type t);
  
  ~Bencode()                              { clear(); }
  
  void                clear();

  Type                get_type() const    { return m_type; }

  bool                is_value() const    { return m_type == TYPE_VALUE; }
  bool                is_string() const   { return m_type == TYPE_STRING; }
  bool                is_list() const     { return m_type == TYPE_LIST; }
  bool                is_map() const      { return m_type == TYPE_MAP; }

  bool                has_key(const std::string& s) const;
  Bencode&            insert_key(const std::string& s, const Bencode& b);
  void                erase_key(const std::string& s);

  int64_t&            as_value();
  std::string&        as_string();
  List&               as_list();
  Map&                as_map();

  int64_t             as_value() const;
  const std::string&  as_string() const;
  const List&         as_list() const;
  const Map&          as_map() const;

  // Unambigious const version of as_*
  int64_t             c_value() const     { return as_value(); }
  const std::string&  c_string() const    { return as_string(); }
  const List&         c_list() const      { return as_list(); }
  const Map&          c_map() const       { return as_map(); }

  Bencode&            operator = (const Bencode& b);
  Bencode&            operator [] (const std::string& k);
  const Bencode&      operator [] (const std::string& k) const;

  friend std::istream& operator >> (std::istream& s, Bencode& b);
  friend std::ostream& operator << (std::ostream& s, const Bencode& b);

 private:
  static bool         read_string(std::istream& s, std::string& str);

  Type                m_type;

  union {
    int64_t             m_value;
    std::string*        m_string;
    List*               m_list;
    Map*                m_map;
  };
};

}

#endif // LIBTORRENT_BENCODE_H
