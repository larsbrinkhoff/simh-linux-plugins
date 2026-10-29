#include <poll.h>
#include <errno.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <alsa/asoundlib.h>

static snd_seq_t *seq;
static snd_seq_addr_t address;

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

  err = snd_seq_open(&seq, "default", SND_SEQ_OPEN_INPUT, SND_SEQ_NONBLOCK);
  if (err < 0)
    fail("Error creating port: %s", snd_strerror(err));

  err = snd_seq_parse_address(seq, &address, port);
  if (err < 0)
    fail("Error parsing port %s: %", port, snd_strerror(err));

  err = snd_seq_create_simple_port(seq, "SIMH MIDI mixer adapter",
                                   SND_SEQ_PORT_CAP_WRITE |
                                   SND_SEQ_PORT_CAP_SUBS_WRITE,
                                   SND_SEQ_PORT_TYPE_MIDI_GENERIC |
                                   SND_SEQ_PORT_TYPE_APPLICATION);
  if (err < 0)
    fail("Error creating port: %s", snd_strerror(err));

  err = snd_seq_connect_from(seq, 0, address.client, address.port);
  if (err < 0)
    fail("Error connecting from port: %s", snd_strerror(err));
}

static void poll_mixer(void)
{
  struct pollfd *fds;
  int n;

  n = snd_seq_poll_descriptors_count(seq, POLLIN);
  fds = alloca(n * sizeof(struct pollfd));
  snd_seq_poll_descriptors(seq, fds, n, POLLIN);
  if (poll(fds, n, -1) == -1)
    fail("Error polling: %s", strerrno());
}

static void write_event(const snd_seq_event_t *event)
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
    return;
  case SND_SEQ_EVENT_NOTEOFF:
    fprintf(stderr, "Note off: channel %d, note %d\n",
            event->data.note.channel,
            event->data.note.note);
    return;
  case SND_SEQ_EVENT_CONTROLLER:
    fprintf(stderr, "Controller change: channel %d, controller %d, value %d\n",
            event->data.control.channel,
            event->data.control.param,
            event->data.control.value);
    switch (event->data.control.param) {
    case 16: index = 0; break;
    case 17: index = 1; break;
    case 20: index = 2; break;
    case 21: index = 3; break;
    case 54: index = 4; break;
    case 55: index = 5; break;
    case 58: index = 6; break;
    case 59: index = 7; break;
    default:
       fprintf(stderr, "Unsupported controller %u\n", event->data.control.param);
       return;
    }
    data[0] = 0x80 + index;
    data[1] = event->data.control.value & 0x7F;
    data[2] = 0x40;
    break;
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

static void usage(const char *name)
{
  fail("Usage: %s [port]", name);
}

int main(int argc, char **argv)
{
  if (argc != 2)
    usage(argv[0]);

  open_mixer(argv[1]);

  fprintf(stderr, "Waiting for data at port %d:0\n", snd_seq_client_id(seq));

  for (;;) {
    snd_seq_event_t *event;
    poll_mixer();
    int err = snd_seq_event_input(seq, &event);
    if (err < 0)
      fail("Error reading event: %s", snd_strerror(err));
    if (event)
      write_event(event);
  }

  return 0;
}
