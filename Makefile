CC=gcc
CPP=g++
LD=$(CPP)

#CFLAGS=-g -Wall -I../../src -fPIC 
CFLAGS=-O3 -funroll-loops -Wall -I../../src `sdl-config --cflags` -Wall -DVERSION_STR=\"$(VERSION_STR)\"
LDFLAGS=`sdl-config --libs` -lm

PROG=vspcplay
include ./shared.mk

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
