#include <iostream>
#include "../rak/socket_address.h"
#include "../src/torrent/object.h"

void
print_addr(const char* name, const rak::socket_address_inet& sa) {
  std::cout << name << ": " << sa.family() << ' ' << sa.address_str() << ':' << sa.port() << std::endl;
}

int
main(int argc, char** argv) {
  rak::socket_address saNone;
  saNone.set_family(rak::socket_address::af_unspec);
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
  sa3.set_family(rak::socket_address::af_inet);
  sa3.sa_inet()->set_port(6999);
  sa3.sa_inet()->set_address_str("127.0.0.2");

  print_addr("sa3", *sa3.sa_inet());

  return 0;
}
