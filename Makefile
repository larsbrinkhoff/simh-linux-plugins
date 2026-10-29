all: ding midi

CFLAGS=-W -Wall
LDLIBS=-lhidapi-libusb -lasound

ding: ding.o

midi: midi.o

clean:
	rm -f ding midi *.o
