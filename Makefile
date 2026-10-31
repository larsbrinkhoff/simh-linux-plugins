all: ding midi midi-panel tester

CFLAGS=-W -Wall
LDLIBS=-lhidapi-libusb -lasound

ding: ding.o

midi: midi.o

midi-panel: midi-panel.o

tester: tester.o

midi-panel.o: linc_panel.h

clean:
	rm -f ding midi midi-panel *.o
