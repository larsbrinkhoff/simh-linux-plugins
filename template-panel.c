#include <poll.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "linc_panel.h"

#define MAX(A,B) ((A) > (B) ? (A) : (B))
#define UNUSED(X) (void)X

struct display {
  void (*display)(void);
};

static unsigned previous_state[PANEL_ITEMS];
static unsigned state[PANEL_ITEMS];

static struct {
  unsigned simh_rx_packet;
  unsigned display;
  unsigned frob[PANEL_ITEMS];
} counters;

static unsigned log_flag;
#define LOG                 (1<<0)
#define LOG_COUNTERS        (LOG | (1<<1))
#define LOG_COUNTERS_DETAIL (LOG | (1<<2))

static void display(void);

static const char *strerrno(void)
{
  return strerror(errno);
}

static void log_(unsigned flag, const char *message)
{
  if ((log_flag & flag) != flag)
    return;
  write(STDERR_FILENO, message, strlen(message));
  fsync(STDERR_FILENO);
}

static void log(unsigned flag, const char *message)
{
  if ((log_flag & flag) != flag)
    return;
  struct timespec now;
  long long msec;
  char buf[1024];
  clock_gettime(CLOCK_MONOTONIC_RAW_APPROX, &now);
  msec = now.tv_nsec / (1000L * 1000L);
  snprintf(buf, sizeof buf, "%ld.%03lld ", now.tv_sec, msec);
  write(STDERR_FILENO, buf, strlen(buf));
  log_(flag, message);
}

static void logf(unsigned flag, const char *format, ...)
{
  if ((log_flag & flag) != flag)
    return;
  va_list ap;
  char buf[1024];
  va_start(ap, format);
  vsnprintf(buf, sizeof buf, format, ap);
  va_end(ap);
  log(flag, buf);
}

static void fail(const char *format, ...)
{
  va_list ap;
  char buf[1024];
  va_start(ap, format);
  vsnprintf(buf, sizeof buf, format, ap);
  va_end(ap);
  log(LOG, buf);
  exit(1);
}

static void clear(void)
{
}

static void nothing(void) {}

static void display_hello(void)
{
  int i;
  counters.display++;
  logf(LOG_COUNTERS, "stats: rx %u, d %u\n", counters.simh_rx_packet, counters.display);
  for (i = 1; i < PANEL_ITEMS; i++) // skip hello (sent every strobe)
    if (counters.frob[i] > 0) {
      logf(LOG_COUNTERS_DETAIL, "frob %d: %u\n", i, counters.frob[i]);
      counters.frob[i] = 0;
    }
}

static void display_on_off(void)
{
  if (state[PANEL_ON_OFF])
    ; // TODO: Overture, curtain, lights
  else {
    clear();
    log(LOG, "bye\n");
    exit(0);
  }
}

static void display_run_pause(void)
{
}

static void display_cycle(void)
{
}

static void display_instruction(void)
{
}

static void display_clear(void)
{
}

static void display_mark(void)
{
}

static void display_relays(void)
{
}

static void display_auto_restart(void)
{
}

static void display_istop(void)
{
}

static void display_xoestop(void)
{
}

static void display_instruction_location(void)
{
}

static void display_sense_switches(void)
{
}

static void display_link(void)
{
}

static void display_accumulator(void)
{
}

static void display_memory_address(void)
{
}

static void display_memory_contents(void)
{
}

static void display_fill_step(void)
{
}

static void display_step_exam(void)
{
}

static void display_fill(void)
{
}

static void display_exam(void)
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

static void ignore(int value, int bit)
{
  UNUSED(value);
  UNUSED(bit);
}

static struct display panel_group[PANEL_ITEMS] = {
  { display_hello                },
  { display_on_off               },
  { display_run_pause            },
  { display_cycle                }, //3
  { display_instruction          },
  { display_clear                },
  { display_mark                 },
  { display_relays               },
  { display_auto_restart         },
  { display_istop                },
  { display_xoestop              }, //10
  { display_instruction_location },
  { display_sense_switches       },
  { nothing                      }, //audio
  { nothing                      }, //delay
  { nothing                      }, //start 400
  { nothing                      }, //start 20
  { nothing                      }, //start rs
  { display_link                 }, //18
  { display_accumulator          },
  { display_memory_address       }, //20
  { display_memory_contents      },
  { display_fill_step            },
  { display_step_exam            },
  { display_fill                 },
  { display_exam                 },
  { display_cycle_by_cycle       },
  { display_instr_by_instr       },
  { display_resume               },
  { display_step                 },
  { display_do_tog_instr         },
  { display_stop                 },
  { display_left_switches        },
  { display_right_switches       },
  { display_gong_on_off          },
  { display_speaker_select       },
  { display_scope_knob_0         },
  { display_scope_knob_1         },
  { display_scope_knob_2         },
  { display_scope_knob_3         },
  { display_scope_knob_4         },
  { display_scope_knob_5         },
  { display_scope_knob_6         },
  { display_scope_knob_7         }
};

static void panel_write(int item)
{
  unsigned char data[3];
  unsigned char *p;
  size_t n;
  ssize_t m;

  state[item] &= 0x3FFFF;

  if (state[item] == previous_state[item])
    return;

  data[0] = 0x80 | item;
  data[1] = state[item] >> 7;
  data[2] = state[item] & 0x7F;

  for (p = data, n = sizeof data; n > 0; p += m, n -= m) {
    m = write(1, p, n);
    if (m <= 0)
      fail("Writing data: %s\n", strerrno());
  }

  logf(LOG, "Wrote item %d %04o\n", item, state[item]);
}

static void frob_on_off(int value)
{
  if (value) {
    state[PANEL_ON_OFF] = 0;
    panel_write(PANEL_ON_OFF);
    clear();
    log(LOG, "bzzt\n");
    exit(0);
  }
}

static void frob_left_switches(int value)
{
  if (value)
    state[PANEL_LEFT_SWITCHES] = value;
  panel_write(PANEL_LEFT_SWITCHES);
  display();
}

static void handle_remote_input(int fd)
{
  UNUSED(fd);
  // TODO: replace this useless toggle with logic based on remote input
  frob_left_switches(0701);
  if (0)
    frob_on_off(0);
}

static void open_panel(const char *address, const char *port)
{
  logf(LOG, "opening panel %s %s\n", address, port);
}

static void display_item(int item)
{
  panel_group[item].display();
}

static void display(void)
{
  int i;
  for (i = 0; i < PANEL_ITEMS; i++)
    display_item(i);
}

static void handle_simh(int fd)
{
  unsigned char data[3];
  unsigned char *p;
  unsigned item, value;
  size_t n;
  ssize_t m;

  do {
    m = read(fd, &data[0], 1);
    if (m == 0)
      return;
    if (m < 0)
      fail("Error reading data: %s\n", strerrno());
  } while ((data[0] & 0x80) == 0);
  for (n = 2, p = data + 1; n > 0; n -= m, p += m) {
    m = read(fd, p, n);
    if (m <= 0)
      fail("Error reading data: %s\n", strerrno());
  }

  if ((data[1] & 0x80) != 0 || (data[2] & 0x80) != 0)
    return;

  item = data[0] & 0x7F;
  value = (data[1] << 7) | data[2];
  state[item] = value;
  counters.simh_rx_packet++;
  counters.frob[item]++;
  display_item(item);
}

static void poll_inputs(void)
{
  unsigned short *revents;
  struct pollfd *fds;
  int i, n, rc;

  n = 1; // TODO: increase to poll other fds for remote input
  fds = alloca(n * sizeof(struct pollfd));
  fds[0].fd = 0;
  fds[0].events = POLLIN;
  rc = poll(fds, n, -1);
  if (rc == -1)
    fail("Error polling: %s\n", strerrno());
  if (fds[0].revents & POLLIN)
    handle_simh(0);
  n--;
  if (n == 0)
    return;
  fds++;
  revents = alloca(n * sizeof(unsigned short));
  for (i = 0; i < n; i++) {
    if ((revents[i] & POLLIN) == 0)
      continue;
    handle_remote_input(fds[i].fd);
  }
}

static void usage(const char *name)
{
  fail("Usage: %s [log-flag] [address] [port]\n", name);
}

int main(int argc, char **argv)
{
  int logfd;

  if (argc != 4)
    usage(argv[0]);

  log_flag = strtoul(argv[1], NULL, 0);
  if (!isatty(2) && log_flag) {
    logfd = open("template-panel.log", O_WRONLY|O_CREAT|O_APPEND, 0644);
    if (logfd == -1) {
      perror("failed to open log");
      return 1;
    }
    if (dup2(logfd, STDERR_FILENO) == -1) {
      perror("failed to redirect stderr");
      close(logfd);
      return 1;
    }
    close(logfd);

  }
  time_t now;
  time(&now);
  logf(LOG, "panel %s", ctime(&now));
  open_panel(argv[2], argv[3]);

  state[PANEL_ON_OFF] = 1;
  display();

  for (;;)
    poll_inputs();

  log(LOG, "done\n");
  return 0;
}
