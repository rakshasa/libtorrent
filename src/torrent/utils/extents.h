#ifndef LIBTORRENT_UTILS_EXTENTS_H
#define LIBTORRENT_UTILS_EXTENTS_H


#include <map>
#include <stdexcept>

namespace torrent {

template <class Address, class Value, class Compare=std::less<Address> >
class extents {
public:
  typedef Address                                  key_type;           // start address
  typedef Value                                    mapped_value_type;  // The value mapped to the ip range
  typedef std::pair<Address, Value>                mapped_type;        // End address, value mapped to ip range
  typedef std::map<key_type, mapped_type, Compare> range_map_type;     // The map itself 

  extents();
  ~extents();

  void              insert(key_type address_start, key_type address_end, mapped_value_type value);
  bool              defined(key_type address_start, key_type address_end);
  bool              defined(key_type address);
  key_type          get_matching_key(key_type address_start, key_type address_end); // throws error on not defined. test with defined() 
  mapped_value_type at(key_type address_start, key_type address_end);               // throws error on not defined. test with defined() 
  mapped_value_type at(key_type address);                                           // throws error on not defined. test with defined()
  unsigned int      sizeof_data() const;

  range_map_type    range_map;
};

///////////////////////////////////////
// CONSTRUCTOR [PLACEHOLDER]
///////////////////////////////////////
template <class Address, class Value, class Compare >
extents<Address, Value, Compare>::extents() {
  //nothing to do
  return;
}

///////////////////////////////////////
// DESTRUCTOR [PLACEHOLDER]
///////////////////////////////////////
template <class Address, class Value, class Compare >
extents<Address, Value, Compare>::~extents() {
  //nothing to do. map destructor can handle cleanup. 
  return;
}

//////////////////////////////////////////////////////////////////////////////////
// INSERT O(log N) assuming no overlapping ranges
/////////////////////////////////////////////////////////////////////////////////
template <class Address, class Value, class Compare >
void extents<Address, Value, Compare>::insert(key_type address_start, key_type address_end, mapped_value_type value) {
  //we allow overlap ranges though not 100% overlap but only if mapped values are the same.  first remove any overlap range that has a different value.
  typename range_map_type::iterator iter = range_map.upper_bound(address_start); 
  if( iter != range_map.begin() ) { iter--; } 
  bool ignore_due_to_total_overlap = false;
  while( iter->first <= address_end && iter != range_map.end() ) {
    key_type delete_key = iter->first;
    bool do_delete_due_to_overlap        =  iter->first <= address_end && (iter->second).first >= address_start && (iter->second).second != value;
    bool do_delete_due_to_total_overlap  =  address_start <= iter->first && address_end >= (iter->second).first;
    iter++;
    if(do_delete_due_to_overlap || do_delete_due_to_total_overlap) {
      range_map.erase (delete_key);
    }
    else if (iter != range_map.end()) {
      ignore_due_to_total_overlap = ignore_due_to_total_overlap || ( iter->first <= address_start && (iter->second).first >= address_end );
    }
  }

  if(!ignore_due_to_total_overlap) {
    mapped_type entry;
    entry.first = address_end;
    entry.second = value;
    range_map.insert( std::pair<key_type,mapped_type>(address_start, entry) );
  }
}

//////////////////////////////////////////////////////////////////////
// DEFINED  O(log N) assuming no overlapping ranges
//////////////////////////////////////////////////////////////////////
template <class Address, class Value, class Compare >
bool extents<Address, Value, Compare>::defined(key_type address_start, key_type address_end) {
  bool defined = false;
  typename range_map_type::iterator iter = range_map.upper_bound(address_start);
  if( iter != range_map.begin() ) { iter--; } 
  while( iter->first <= address_end && !defined && iter != range_map.end() ) {
    defined = iter->first <= address_end && (iter->second).first >= address_start;
    iter++;
  }
  return defined;
}
template <class Address, class Value, class Compare >
bool extents<Address, Value, Compare>::defined(key_type address) {
  return defined(address, address);
}

//////////////////////////////////////////////////////////////////////
// GET_MATCHING_KEY  O(log N) assuming no overlapping ranges
//////////////////////////////////////////////////////////////////////
template <class Address, class Value, class Compare >
typename extents<Address, Value, Compare>::key_type extents<Address, Value, Compare>::get_matching_key(key_type address_start, key_type address_end) {
  key_type key;
  bool defined = false;
  typename range_map_type::iterator iter = range_map.upper_bound(address_start);
  if( iter != range_map.begin() ) { iter--; } 
  while( iter->first <= address_end && !defined && iter != range_map.end() ) {
    defined = iter->first <= address_end && (iter->second).first >= address_start;
    if(defined)
      key = iter->first;
    
    iter++;
  }
  // this will cause exception to be thrown 
  if(!defined) {
    std::out_of_range e("nothing defined for specified key");
    throw e;
  }
  return key;
}

//////////////////////////////////////////////////////////////////////
// AT  O(log N) assuming no overlapping ranges
//////////////////////////////////////////////////////////////////////
template <class Address, class Value, class Compare >
typename extents<Address, Value, Compare>::mapped_value_type extents<Address, Value, Compare>::at(key_type address_start, key_type address_end) {
  key_type key = get_matching_key(address_start, address_end);
  mapped_type entry = range_map.at(key);
  return entry.second;
}
template <class Address, class Value, class Compare >
typename extents<Address, Value, Compare>::mapped_value_type extents<Address, Value, Compare>::at(key_type address) {
  return at(address, address);
}

//////////////////////////////////////////////////////////////////////
// SIZEOF_DATA  O(1)
//////////////////////////////////////////////////////////////////////
template <class Address, class Value, class Compare >
unsigned int extents<Address, Value, Compare>::sizeof_data() const {
  // we don't know overhead on map, so this won't be accurate.  just estimate.
  unsigned int entry_size = sizeof(key_type) + sizeof(mapped_type);
  return entry_size * range_map.size();
}


}

#endif
