#include <poll.h>
#include <errno.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <alsa/asoundlib.h>
#include "linc_panel.h"

static snd_seq_t *seq;
static snd_seq_addr_t address;
static snd_rawmidi_t *output;
static unsigned lights = 0;
static unsigned state[PANEL_ITEMS];

static void note_onoff(int i, int on);

static const char *strerrno(void)
{
  return strerror(errno);
}

static void fail(const char *format, ...)
{
  va_list ap;
  va_start(ap, format);
  vfprintf(stderr, format, ap);
  fputc('\n', stderr);
  fflush(stderr);
  va_end(ap);
  exit(1);
}

static void open_mixer(const char *port)
{
  int err;

  err = snd_seq_open(&seq, "default", SND_SEQ_OPEN_DUPLEX, 0);
  if (err < 0)
    fail("Error creating port: %s", snd_strerror(err));

  snd_seq_set_client_name(seq, "LINC panel");

  err = snd_seq_parse_address(seq, &address, port);
  if (err < 0)
    fail("Error parsing port %s: %", port, snd_strerror(err));

  err = snd_seq_create_simple_port(seq, "SIMH MIDI mixer adapter",
                                   SND_SEQ_PORT_CAP_READ |
                                   SND_SEQ_PORT_CAP_WRITE |
                                   SND_SEQ_PORT_CAP_SUBS_READ |
                                   SND_SEQ_PORT_CAP_SUBS_WRITE,
                                   SND_SEQ_PORT_TYPE_MIDI_GENERIC |
                                   SND_SEQ_PORT_TYPE_APPLICATION);
  if (err < 0)
    fail("Error creating port: %s", snd_strerror(err));

  err = snd_seq_connect_from(seq, 0, address.client, address.port);
  if (err < 0)
    fail("Error connecting from port: %s", snd_strerror(err));
}

static void display(void)
{
  static const int button[12] =
    { 1, 4, 7, 10, 13, 16,
      3, 6, 9, 12, 15, 18 };
  unsigned mask = 04000;
  unsigned value = state[lights];
  int i;
  for (i = 0; i < 12; i++, mask >>= 1)
    note_onoff(button[i], value & mask);
}

static void write_event(snd_seq_event_t *event)
{
  unsigned char data[3];
  unsigned char *p;
  unsigned char index;
  size_t n;
  ssize_t m;

  switch(event->type) {
  case SND_SEQ_EVENT_NOTEON:
    fprintf(stderr, "Note on: channel %d, note %d\n",
            event->data.note.channel,
            event->data.note.note);
    switch (event->data.note.note) {
    case 3:
    case 6:
    case 9:
    case 12:
    case 15:
    case 18:
      event->data.note.note -= 2;
    }
    switch (event->data.note.note) {
    case 1: index = 0; break;
    case 4: index = 1; break;
    case 7: index = 2; break;
    case 10: index = 3; break;
    case 13: index = 4; break;
    case 16: index = 5; break;
    default:
      fprintf(stderr, "Unsupported note %u\n", event->data.note.note);
      return;
    }
    note_onoff(event->data.note.note, 1);
    data[0] = 0x80 + index;
    data[1] = 1;
    data[2] = 0;
    break;
  case SND_SEQ_EVENT_NOTEOFF:
    fprintf(stderr, "Note off: channel %d, note %d\n",
            event->data.note.channel,
            event->data.note.note);
    switch (event->data.note.note) {
    case 1: index = 0; break;
    case 4: index = 1; break;
    case 7: index = 2; break;
    case 10: index = 3; break;
    case 13: index = 4; break;
    case 16: index = 5; break;
    default:
      fprintf(stderr, "Unsupported note %u\n", event->data.note.note);
      return;
    }
    note_onoff(event->data.note.note, 0);
    data[0] = 0x80 + index;
    data[1] = 0;
    data[2] = 0;
    break;
  case SND_SEQ_EVENT_CONTROLLER:
    fprintf(stderr, "Controller change: channel %d, controller %d, value %d\n",
            event->data.control.channel,
            event->data.control.param,
            event->data.control.value);
    switch (event->data.control.param) {
    case 18:
      lights = event->data.control.value >> 2;
      fprintf(stderr, "Lights: %u\n", lights);
      display();
      return;
    }
    return;
  default:
    fprintf(stderr, "Other\n");
    return;
  }
  fflush(stderr);

  for (p = data, n = sizeof data; n > 0; p += m, n -= m) {
    m = write(1, p, n);
    if (m <= 0)
      fail("Writing data: %s", strerrno());
  }
}

static void handle_simh(void)
{
  unsigned char data[3];
  unsigned char *p;
  unsigned item, value;
  size_t n;
  ssize_t m;
  
  do {
    if (read(0, &data[0], 1) <= 0)
      fail("Error reading data: %s", strerrno());
  } while ((data[0] & 0x80) == 0);

  for (n = 2, p = data + 1; n > 0; n -= m, p += m) {
    m = read(0, p, n);
    if (m <= 0)
      fail("Error reading data: %s", strerrno());
  }

  if ((data[1] & 0x80) != 0 || (data[2] & 0x80) != 0)
    return;

  item = data[0] & 0x7F;
  value = (data[1] << 7) | data[2];
  state[item] = value;

  if (item == lights)
    display();
}

static void poll_inputs(void)
{
  snd_seq_event_t *event;
  unsigned short *revents;
  struct pollfd *fds;
  int i, n, err;

  n = 1 + snd_seq_poll_descriptors_count(seq, POLLIN);
  fds = alloca(n * sizeof(struct pollfd));
  fds[0].fd = 0;
  fds[0].events = POLLIN;
  snd_seq_poll_descriptors(seq, fds + 1, n, POLLIN);
  if (poll(fds, n, -1) == -1)
    fail("Error polling: %s", strerrno());
  if (fds[0].revents & POLLIN)
    handle_simh();
  n--;
  fds++;
  revents = alloca(n * sizeof(unsigned short));
  snd_seq_poll_descriptors_revents(seq, fds, n, revents);
  for (i = 0; i < n; i++) {
    if ((revents[i] & POLLIN) == 0)
      continue;
    err = snd_seq_event_input(seq, &event);
    if (err < 0)
      fail("Error reading event: %s", snd_strerror(err));
    if (event)
      write_event(event);
  }
}

static void usage(const char *name)
{
  fail("Usage: %s [port]", name);
}

static void note_onoff(int i, int on)
{
  unsigned char data[3] = { 0x90, i, on ? 1 : 0 };
  int err = snd_rawmidi_write(output, data, sizeof data);
  if (err < 0)
    fail("Sending raw MIDI: %s", snd_strerror(err));

#if 0

  snd_seq_event_t event;
  int err;

  fprintf(stderr, "Note %d %s\n", i, on ? "on" : "off");
  snd_seq_ev_clear(&event);
  if (on)
    snd_seq_ev_set_noteon(&event, 0, i, 127);
  else
    snd_seq_ev_set_noteoff(&event, 0, i, 0);
  snd_seq_ev_set_direct(&event);
  snd_seq_ev_set_subs(&event);
  snd_seq_ev_set_source(&event, 0);
  //snd_seq_ev_set_dest(&event, address.client, address.port);
  err = snd_seq_event_output_direct(seq, &event);
  if (err < 0)
    fail("Error sending event: %s", snd_strerror(err));
#endif
}

static void lights_off(void)
{
  int i;
  for (i = 1; i < 28; i++)
    note_onoff(i, 0);
}

int main(int argc, char **argv)
{
  if (!isatty(2))
    stderr = fopen("log", "w");

  if (argc != 3)
    usage(argv[0]);

  open_mixer(argv[1]);

  int err;
  snd_rawmidi_t **ptr;
  ptr = &output;
  err = snd_rawmidi_open(NULL, ptr, argv[2], SND_RAWMIDI_NONBLOCK);
  if (err < 0)
    fail("Opening raw MIDI: %s", snd_strerror(err));
  err = snd_rawmidi_nonblock(output, 0);
  if (err < 0)
    fail("Setting raw MIDI non blocking: %s", snd_strerror(err));

  lights_off();

  fprintf(stderr, "Reading MIDI events from %d:0, sending to %s\n",
          snd_seq_client_id(seq), argv[2]);
  fflush(stderr);

  for (;;)
    poll_inputs();

  return 0;
}
