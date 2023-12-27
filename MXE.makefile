MXEBASE=/opt/mxe/mxe
CC=$(MXEBASE)/usr/bin/i686-w64-mingw32.static-gcc
CPP=$(MXEBASE)/usr/bin/i686-w64-mingw32.static-g++
SDLCONFIG=$(MXEBASE)/usr/i686-w64-mingw32.static/bin/sdl-config
LD=$(CPP)

#CFLAGS=-g -Wall -I../../src -fPIC
CFLAGS=-O3 -funroll-loops -Wall -I../../src `$(SDLCONFIG) --cflags` -Wall -DVERSION_STR=\"1.5\"
LDFLAGS=`$(SDLCONFIG) --libs` -lm

PROG=vspcplay.exe

OBJS = apu.o globals.o libspc.o soundux.o spc700.o main.o font.o sdlfont.o id666.o wavewriter.o

all: $(PROG)

$(PROG): $(OBJS)
	$(LD) $(OBJS) -o $(PROG) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $<

%.o: %.c %.h
	$(CC) $(CFLAGS) -c $<

%.o: %.cpp %.h
	$(CPP) $(CFLAGS) -c $<

%.o: %.cpp
	$(CPP) $(CFLAGS) -c $<

clean:
	rm -f *.o $(PROG)
