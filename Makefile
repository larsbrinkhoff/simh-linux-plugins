.PHONY: all
all: ding midi midi-panel template-panel

CFLAGS=-W -Wall

ding: ding.o
	$(CC) $(CFLAGS) -o $@ $^ -lhidapi-libusb

midi: midi.o
	$(CC) $(CFLAGS) -o $@ $^ -lasound

midi-panel: midi-panel.o
	$(CC) $(CFLAGS) -o $@ $^ -lasound

template-panel: template-panel.o
	$(CC) $(CFLAGS) -o $@ $^

midi-panel.o: linc_panel.h

template-panel.o: linc_panel.h

.PHONY: clean
clean:
	rm -f ding midi midi-panel template-panel *.o
