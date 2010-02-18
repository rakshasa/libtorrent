#include "torrent/object.h"

torrent::Object create_bencode(const char* str);

bool compare_bencode(const torrent::Object& obj, const char* str, uint32_t skip_mask = 0);
