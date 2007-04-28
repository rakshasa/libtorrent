#include <iostream>
#include <iomanip>
#include <rak/timer.h>
#include "../src/torrent/object_stream.h"

#ifdef NEW_OBJECT
#include "object.h"

typedef torrent::Object return_type;
//#define OBJECTREF_MOVE(x) torrent::ObjectRef::move(x)
#define OBJECTREF_MOVE(x) x

#else
#include "../src/torrent/object.h"

typedef torrent::Object return_type;
#define OBJECTREF_MOVE(x) x

#endif

#define TIME_WRAPPER(name, body)                \
rak::timer                                      \
time_##name(unsigned int n) {                   \
  rak::timer started = rak::timer::current();   \
                                                \
  for (unsigned int i = 0; i < n; i++) {        \
    body;                                       \
  }                                             \
                                                \
  return rak::timer::current() - started;       \
}

typedef std::list<std::string> std_list_type;

void f() {}
torrent::Object func_create_string_20() { return torrent::Object("12345678901234567890"); }
std::string     func_create_std_string_20() { return "12345678901234567890"; }

return_type
func_create_string_list_20() {
  torrent::Object tmp(torrent::Object::TYPE_LIST);
  torrent::Object::list_type& listRef = tmp.as_list();
  for (int i = 0; i < 10; i++) listRef.push_back(torrent::Object("12345678901234567890"));
  return OBJECTREF_MOVE(tmp);
}

std_list_type
func_create_std_string_list_20() {
  std_list_type tmp(torrent::Object::TYPE_LIST);
  for (int i = 0; i < 10; i++) tmp.push_back("12345678901234567890");
  return tmp;
}

torrent::Object stringList20(func_create_string_list_20());

// return_type
// func_copy_string_list_20_f() {
//   torrent::Object tmp(stringList20);

//   return OBJECTREF_MOVE(tmp);
// }

torrent::Object tmp1;

return_type
func_copy_string_list_20() {
  tmp1 = stringList20;

  return OBJECTREF_MOVE(tmp1);
}

TIME_WRAPPER(dummy, f(); )
TIME_WRAPPER(string, torrent::Object s("12345678901234567890"); )
TIME_WRAPPER(std_string, std::string s("12345678901234567890"); )

TIME_WRAPPER(return_string,     torrent::Object s = func_create_string_20(); )
TIME_WRAPPER(return_std_string, std::string s = func_create_std_string_20(); )

TIME_WRAPPER(return_string_list,      torrent::Object s(func_create_string_list_20()); )
TIME_WRAPPER(return_std_string_list,  std_list_type   s(func_create_std_string_list_20()); )
TIME_WRAPPER(copy_string_list,        torrent::Object s(func_copy_string_list_20()); )

int
main(int argc, char** argv) {
//   std::cout << "sizeof(torrent::Object): " << sizeof(torrent::Object) << std::endl;
//   std::cout << "sizeof(torrent::Object::value_type): " << sizeof(torrent::Object::value_type) << std::endl;
//   std::cout << "sizeof(torrent::Object::string_type): " << sizeof(torrent::Object::string_type) << std::endl;
//   std::cout << "sizeof(torrent::Object::map_type): " << sizeof(torrent::Object::map_type) << std::endl;
//   std::cout << "sizeof(torrent::Object::list_type): " << sizeof(torrent::Object::list_type) << std::endl;

  std::cout.setf(std::ios::right, std::ios::adjustfield);

  std::cout << "time_dummy:                  " << std::setw(8) << time_dummy(100000).usec() << std::endl;
  std::cout << "time_string:                 " << std::setw(8) << time_string(100000).usec() << std::endl;
  std::cout << "time_std_string:             " << std::setw(8) << time_std_string(100000).usec() << std::endl;
  std::cout << "time_return_string:          " << std::setw(8) << time_return_string(100000).usec() << std::endl;
  std::cout << "time_return_std_string:      " << std::setw(8) << time_return_std_string(100000).usec() << std::endl;
  std::cout << std::endl;
  std::cout << "time_return_string_list:     " << std::setw(8) << time_return_string_list(100000).usec() << std::endl;
  std::cout << "time_return_std_string_list: " << std::setw(8) << time_return_std_string_list(100000).usec() << std::endl;
  std::cout << "time_copy_string_list:       " << std::setw(8) << time_copy_string_list(100000).usec() << std::endl;

  return 0;
}
