#ifndef LIBTORRENT_BENCODE_H
#define LIBTORRENT_BENCODE_H

#include <iosfwd>
#include <string>
#include <map>
#include <list>

namespace torrent {

class bencode {
 public:
  typedef std::list<bencode> List;
  typedef std::map<std::string, bencode> Map;

  enum Type {
    TYPE_NONE,
    TYPE_VALUE,
    TYPE_STRING,
    TYPE_LIST,
    TYPE_MAP
  };

  bencode();
  bencode(const bencode& b);

  bencode(const int64_t v);
  bencode(const std::string& s);
  bencode(const char* s);

  ~bencode();
  
  Type type() const     { return m_type; }
  bool isValue() const  { return m_type == TYPE_VALUE; }
  bool isString() const { return m_type == TYPE_STRING; }
  bool isList() const   { return m_type == TYPE_LIST; }
  bool isMap() const    { return m_type == TYPE_MAP; }

  int64_t&           asValue();
  std::string&       asString();
  List&              asList();
  Map&               asMap();

  int64_t            asValue() const;
  const std::string& asString() const;
  const List&        asList() const;
  const Map&         asMap() const;

  bencode& operator = (const bencode& b);
  bencode& operator [] (const std::string& k);
  const bencode& operator [] (const std::string& k) const;

  bool hasKey(const std::string& s) const;

  friend std::istream& operator >> (std::istream& s, bencode& b);
  friend std::ostream& operator << (std::ostream& s, const bencode& b);

  // Unambigious constness.
  int64_t            nValue() { return asValue(); }
  const std::string& nString() { return asString(); }
  const List&        nList() { return asList(); }
  const Map&         nMap() { return asMap(); }
  
  int64_t            cValue() const { return asValue(); }
  const std::string& cString() const { return asString(); }
  const List&        cList() const { return asList(); }
  const Map&         cMap() const { return asMap(); }

 private:
  void clear();
  static bool readString(std::istream& s, std::string* str);

  Type m_type;

  union {
    int64_t m_value;
    std::string* m_string;
    List* m_list;
    Map* m_map;
  };
};

}

#endif // LIBTORRENT_BENCODE_H
