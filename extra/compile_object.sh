STUFF="-Wall -O2 -I.. -I../src/ -lcrypto `pkg-config --libs-only-L openssl` `pkg-config --cflags openssl`"

g++ $STUFF -DNEW_OBJECT -o test_object test_object.cc object.cc ../src/torrent/object_stream.cc ../src/torrent/exceptions.cc
g++ $STUFF -DOLD_OBJECT -o test_object_old test_object.cc ../src/torrent/object.cc ../src/torrent/object_stream.cc ../src/torrent/exceptions.cc
