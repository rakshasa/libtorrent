OBJECTS = http.o display.o download.o rtorrent.o

LIBS_TMP = `pkg-config --libs sigc++-2.0`
LIBS = -ltorrent -lncurses -lcrypto $(LIBS_TMP)

CFLAGS_TMP = `pkg-config --cflags sigc++-2.0`
CFLAGS = -Wall -O3 -g $(CFLAGS_TMP)

all: $(OBJECTS)
	g++ $(CFLAGS) -o rtorrent $(OBJECTS) $(LIBS)

%.o: %.cc
	g++ $(CFLAGS) -c $<

clean:
	-rm rtorrent *.o *~
