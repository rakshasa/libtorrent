OBJECTS = http.o curl_get.o curl_stack.o display.o download.o queue.o rtorrent.o

LIBS = -lncurses `pkg-config --libs sigc++-2.0 libtorrent` `curl-config --libs`
CFLAGS = -Wall -O3 -g `pkg-config --cflags sigc++-2.0 libtorrent` `curl-config --cflags` -I..

all: tests $(OBJECTS)
	g++ $(CFLAGS) -o rtorrent $(OBJECTS) $(LIBS)

tests:
	@curl-config --version
	pkg-config --exists sigc++-2.0 libtorrent

%.o: %.cc
	g++ $(CFLAGS) -c $<

clean:
	-rm rtorrent *.o *~
