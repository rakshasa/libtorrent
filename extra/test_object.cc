#include <iostream>
#include "../src/torrent/object.h"

int
test_construct_value() {
  torrent::Object o1(0);

  return o1.as_value();
}

torrent::Object
test_construct_string() {
  //torrent::Object o1(std::string());
  torrent::Object o1(torrent::Object::TYPE_STRING);

  return o1;
}

torrent::Object
test_construct_string_2() {
  return torrent::Object("test_construct_string_2()");
}

torrent::Object test_weak_string_o1("test_weak_string()");
torrent::Object test_weak_string_o2("test_weak_string_2()");

torrent::WeakObject
test_weak_string() {
  return test_weak_string_o1;
}

torrent::WeakObject
test_weak_string_2() {
  return test_weak_string_o2;
}

torrent::Object
test_map() {
  torrent::Object o1 = torrent::Object::map_type();

  // Allow direct copy of string?
  o1.as_map()["test"] = torrent::Object("test_map()");

  return o1;
}

int
main(int argc, char** argv) {
  std::cout << "sizeof(torrent::Object): " << sizeof(torrent::Object) << std::endl;
  std::cout << "sizeof(torrent::Object::value_type): " << sizeof(torrent::Object::value_type) << std::endl;
  std::cout << "sizeof(torrent::Object::string_type): " << sizeof(torrent::Object::string_type) << std::endl;
  std::cout << "sizeof(torrent::Object::map_type): " << sizeof(torrent::Object::map_type) << std::endl;
  std::cout << "sizeof(torrent::Object::list_type): " << sizeof(torrent::Object::list_type) << std::endl;

  test_construct_value();
  test_construct_string();

  std::cout << test_construct_string_2().as_string() << std::endl;
  std::cout << test_weak_string().as_string() << std::endl;

  torrent::Object o1 = test_weak_string();

  std::cout << o1.as_string() << std::endl;

  std::cout << test_map().as_map()["test"].as_string() << std::endl;

  return 0;
}
