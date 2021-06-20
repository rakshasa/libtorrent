#import <torrent/common.h>
#import <torrent/net/socket_address.h>

namespace torrent {

auto detect_local_sin_addr() -> sin_unique_ptr;
auto detect_local_sin6_addr() -> sin6_unique_ptr;

}
