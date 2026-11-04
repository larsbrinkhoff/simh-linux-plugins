#include <poll.h>
#include <errno.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <alsa/asoundlib.h>
#include "linc_panel.h"

#define MAX(A,B) ((A) > (B) ? (A) : (B))

static snd_seq_t *seq;
static snd_seq_addr_t address;
static snd_rawmidi_t *output;
static unsigned state[PANEL_ITEMS];
static int items_in_group[5];
static int current_group;
static int current_selection;

static void note_onoff(int i, int on);

static void display_top_row(unsigned data, unsigned bits, unsigned start)
{
  static const int button[8] =
    { 1, 4, 7, 10, 13, 16, 19, 21 };
  unsigned mask = 1 << (bits - 1);
  unsigned i;
  for (i = start; i < start + bits; i++, mask >>= 1)
    note_onoff(button[i], data & mask);
}

static void display_bottom_row(unsigned data, unsigned bits, unsigned start)
{
  static const int button[8] =
    { 3, 6, 9, 12, 15, 18, 21, 24 };
  unsigned mask = 1 << (bits - 1);
  unsigned i;
  for (i = start; i < start + bits; i++, mask >>= 1)
    note_onoff(button[i], data & mask);
}

static void display_nothing(void) {}

static void display_on_off(void)
{
  display_top_row(state[PANEL_ON_OFF], 1, 7);
}

static void display_run_pause(void)
{
  display_top_row(state[PANEL_RUN_PAUSE], 2, 5);
}

static void display_cycle(void)
{
  display_top_row(state[PANEL_CYCLE], 2, 3);
}

static void display_instruction(void)
{
  display_top_row(state[PANEL_INSTRUCTION] >> 6, 6, 0);
  display_bottom_row(state[PANEL_INSTRUCTION], 6, 0);
}

static void display_clear(void)
{
  display_top_row(state[PANEL_CLEAR], 1, 1);
}

static void display_mark(void)
{
  display_top_row(state[PANEL_MARK], 1, 0);
}

static void display_relays(void)
{
  display_bottom_row(state[PANEL_RELAYS], 6, 0);
}

static void display_auto_restart(void)
{
  display_top_row(state[PANEL_AUTO_RESTART], 1, 7);
}

static void display_istop(void)
{
  display_top_row(state[PANEL_ISTOP], 1, 5);
}

static void display_xoestop(void)
{
  display_top_row(state[PANEL_XOESTOP], 1, 4);
}

static void display_instruction_location(void)
{
  display_top_row(state[PANEL_INSTRUCTION_LOCATION] >> 6, 4, 6);
  display_bottom_row(state[PANEL_INSTRUCTION_LOCATION], 6, 0);
}

static void display_sense_switches(void)
{
  display_top_row(state[PANEL_SENSE_SWITCHES], 6, 0);
}

#if 0
static void audio(void) { }
  // delay
static void start(void) { }
static void start(void) { }
static void start(void) { }
#endif

static void display_link(void)
{
  display_top_row(state[PANEL_LINK], 1, 6);
}

static void display_accumulator(void)
{
  display_top_row(state[PANEL_ACCUMULATOR] >> 6, 6, 6);
  display_bottom_row(state[PANEL_ACCUMULATOR], 6, 0);
}

static void display_memory_address(void)
{
  display_top_row(state[PANEL_MEMORY_ADDRESS] >> 6, 6, 6);
  display_bottom_row(state[PANEL_MEMORY_ADDRESS], 6, 0);
}

static void display_memory_contents(void)
{
  display_top_row(state[PANEL_MEMORY_ADDRESS] >> 6, 6, 6);
  display_bottom_row(state[PANEL_MEMORY_ADDRESS], 6, 0);
}

static void display_fill_step(void)
{
}

static void display_exam_step(void)
{
}

static void display_cycle_by_cycle(void)
{
}

static void display_instr_by_instr(void)
{
}

static void display_resume(void)
{
}

static void display_step(void)
{
}

static void display_do_tog_instr(void)
{
}

static void display_stop(void)
{
}

static void display_left_switches(void)
{
}

static void display_right_switches(void)
{
}

static void display_gong_on_off(void)
{
}

static void display_speaker_select(void)
{
}

static void display_scope_knob_0(void)
{
}

static void display_scope_knob_1(void)
{
}

static void display_scope_knob_2(void)
{
}

static void display_scope_knob_3(void)
{
}

static void display_scope_knob_4(void)
{
}

static void display_scope_knob_5(void)
{
}

static void display_scope_knob_6(void)
{
}

static void display_scope_knob_7(void)
{
}

struct {
  void (*display)(void);
  int group;
  int selection;
} panel_group[PANEL_ITEMS] = {
  { display_nothing,              0, 0 },
  { display_on_off,               1, 0 },
  { display_run_pause,            1, 0 },
  { display_cycle,                1, 0 },
  { display_instruction,          1, 1 },
  { display_clear,                1, 2 },
  { display_mark,                 1, 2 },
  { display_relays,               1, 2 },
  { display_auto_restart,         2, 0 },
  { display_istop,                2, 0 },
  { display_xoestop,              2, 0 },
  { display_instruction_location, 2, 1 },
  { display_sense_switches,       2, 2 },
  // audio
  // delay
  // start 400
  // start 200
  // start rs
  { display_link,                 3, 0 },
  { display_accumulator,          3, 0 },
  { display_memory_address,       3, 1 },
  { display_memory_contents,      3, 2 },
  { display_fill_step,            4, 0 },
  { display_exam_step,            4, 0 },
  { display_cycle_by_cycle,       4, 0 },
  { display_instr_by_instr,       4, 0 },
  { display_resume,               4, 0 },
  { display_step,                 4, 0 },
  { display_do_tog_instr,         4, 0 },
  { display_stop,                 4, 0 },
  { display_left_switches,        4, 1 },
  { display_right_switches,       4, 2 },
  { display_gong_on_off,          5, 0 },
  { display_speaker_select,       5, 0 },
  { display_scope_knob_0,         5, 0 },
  { display_scope_knob_1,         5, 0 },
  { display_scope_knob_2,         5, 0 },
  { display_scope_knob_3,         5, 0 },
  { display_scope_knob_4,         5, 0 },
  { display_scope_knob_5,         5, 0 },
  { display_scope_knob_6,         5, 0 },
  { display_scope_knob_7,         5, 0 }
};

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
  int i;
  for (i = 0; i < PANEL_ITEMS; i++) {
    if (panel_group[i].group == current_group &&
        panel_group[i].selection == current_selection)
      panel_group[i].display();
  }
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
      current_group = 1;
      break;
    case 21:
      current_group = 2;
      break;
    case 24:
      current_group = 3;
      break;
    case 27:
      current_group = 4;
      break;
    case 30:
      current_group = 5;
      break;
    }
    current_selection = event->data.control.value /
      ((127 + items_in_group[current_group]) /
       items_in_group[current_group]);
    fprintf(stderr, "Group: %d, selection: %d\n",
            current_group, current_selection);
    display();
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

  if (panel_group[item].group == current_group &&
      panel_group[item].selection == current_selection)
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
}

static void lights_off(void)
{
  int i;
  for (i = 1; i < 28; i++)
    note_onoff(i, 0);
}

int main(int argc, char **argv)
{
  unsigned i;

  if (!isatty(2))
    stderr = fopen("log", "w");

  if (argc != 3)
    usage(argv[0]);

  memset(items_in_group, 0, sizeof items_in_group);
  for (i = 0; i < sizeof panel_group / sizeof panel_group[0]; i++) {
    int group = panel_group[i].group;
    items_in_group[group] = MAX(items_in_group[group],
                                panel_group[i].selection + 1);
  }

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
