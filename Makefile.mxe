# Figure out MXEBase, as a user can install MXE to /opt/mxe/mxe (manual install) or /usr/lib/mxe (APT install or manual install).

ifdef MXEBASE

# MXEBASE defined via CLI arguments.
$(info Using MXE from custom directory '$(MXEBASE)'.)

else ifneq (,$(wildcard /opt/mxe/mxe))

# MXE in /opt/mxe/mxe.
MXEBASE=/opt/mxe/mxe
$(warning Using MXE from '/opt/mxe/mxe'.)

else ifneq (,$(wildcard /usr/lib/mxe))

# MXE in /usr/lib/mxe.
MXEBASE=/usr/lib/mxe
$(warning Using MXE from '/usr/lib/mxe'.)

else

# No installation found.
$(error No MXE installation found. Please install MXE to '/opt/mxe/mxe' or '/usr/lib/mxe' or use "MXEBASE=<path>" in the command line arguments.)

endif

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
