OBJECTS = http.o display.o download.o rtorrent.o
CFLAGS = -Wall -O3 -g -I..
LIBS = -L../torrent/.libs -ltorrent -lncurses -lcrypto

all: $(OBJECTS)
	g++ $(CFLAGS) -o rtorrent $(OBJECTS) $(LIBS)

%.o: %.cc
	g++ $(CFLAGS) -c $<

clean:
	-rm rtorrent *.o *~
