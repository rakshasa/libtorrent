OBJECTS = http.o display.o download.o queue.o rtorrent.o

LIBS = -ltorrent -lncurses -lcrypto `pkg-config --libs sigc++-2.0 libtorrent`

CFLAGS = -Wall -O3 -g `pkg-config --cflags sigc++-2.0 libtorrent`

all: $(OBJECTS)
	g++ $(CFLAGS) -o rtorrent $(OBJECTS) $(LIBS)

%.o: %.cc
	g++ $(CFLAGS) -c $<

clean:
	-rm rtorrent *.o *~
