#include <iostream>
#include <rak/address_info.h>
#include "../src/torrent/object.h"

void
print_addr(const char* name, const rak::socket_address_inet& sa) {
  std::cout << name << ": " << sa.family() << ' ' << sa.address_str() << ':' << sa.port() << std::endl;
}

bool
lookup_address(const char* name) {
  rak::address_info* result;

  std::cout << "Lookup: " << name << std::endl;

//   int errcode = rak::address_info::get_address_info(name, 0, 0, &result);
  int errcode = rak::address_info::get_address_info(name, PF_INET6, 0, &result);

  if (errcode != 0) {
    std::cout << "Failed: " << rak::address_info::strerror(errcode) << std::endl;
    return false;
  }

  for (rak::address_info* itr = result; itr != NULL; itr = itr->next()) {
    std::cout << "Flags: " << itr->flags() << std::endl;
    std::cout << "Family: " << itr->family() << std::endl;
    std::cout << "Socket Type: " << itr->socket_type() << std::endl;
    std::cout << "Protocol: " << itr->protocol() << std::endl;
    std::cout << "Length: " << itr->length() << std::endl;

    std::cout << "Address: " << itr->address()->family() << ' ' << itr->address()->address_str() << ':' << itr->address()->port() << std::endl;
  }

  // Release.
  freeaddrinfo(reinterpret_cast<addrinfo*>(result));

  return true;
}

int
main(int argc, char** argv) {
  std::cout << "sizeof(sockaddr_in): " << sizeof(sockaddr_in) << std::endl;
  std::cout << "sizeof(sockaddr_in6): " << sizeof(sockaddr_in6) << std::endl;

  rak::socket_address saNone;
  saNone.set_family();
  print_addr("none", *saNone.sa_inet());

  rak::socket_address_inet sa1;
  sa1.set_family();
  sa1.set_port(0);
  sa1.set_address_any();

  print_addr("sa1", sa1);

  rak::socket_address_inet sa2;
  sa2.set_family();
  sa2.set_port(12345);

  if (!sa2.set_address_str("123.45.67.255"))
    return -1;

  print_addr("sa2", sa2);

  rak::socket_address sa3;
  sa3.sa_inet()->set_family();
  sa3.sa_inet()->set_port(6999);
  sa3.sa_inet()->set_address_str("127.0.0.2");

  print_addr("sa3", *sa3.sa_inet());

  lookup_address("www.uio.no");
  lookup_address("www.ipv6.org");

  return 0;
}
