/**
 * Thanks to this stack-overflow question : 
 *   https://stackoverflow.com/questions/7675379/how-to-control-mouse-movement-in-linux
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/input.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <string.h>

#include "mouse.h"

static void send_event(int fd, int type, int code, int value)
{
  struct input_event event, event_end;
  memset(&event, 0, sizeof(event));
  memset(&event_end, 0, sizeof(event_end));

  gettimeofday(&event.time, NULL);
  event.type = type;
  event.code = code;
  event.value = value;
  gettimeofday(&event_end.time, NULL);
  event_end.type = EV_SYN;
  event_end.code = SYN_REPORT;
  event_end.value = 0;

  write(fd, &event, sizeof(event));// Move the mouse
  write(fd, &event_end, sizeof(event_end));// Show move

}

void mouse_send_wheel(int fd, int value)
{
  send_event(fd, EV_REL, REL_WHEEL, value);
}

void mouse_send_lmb(int fd, int value)
{
  send_event(fd, EV_KEY, BTN_LEFT, value);
}

void mouse_move_relative(int fd, int x, int y)
{
  struct input_event event_x, event_y, event_end;
  memset(&event_x, 0, sizeof(event_x));
  memset(&event_y, 0, sizeof(event_y));
  memset(&event_end, 0, sizeof(event_end));

  gettimeofday(&event_x.time, NULL);
  event_x.type = EV_REL;
  event_x.code = REL_X;
  event_x.value = x;
  gettimeofday(&event_y.time, NULL);
  event_y.type = EV_REL;
  event_y.code = REL_Y;
  event_y.value = y;
  gettimeofday(&event_end.time, NULL);
  event_end.type = EV_SYN;
  event_end.code = SYN_REPORT;
  event_end.value = 0;

  write(fd, &event_x, sizeof(event_x));// Move the mouse
  write(fd, &event_y, sizeof(event_y));// Move the mouse
  write(fd, &event_end, sizeof(event_end));// Show move
}

int mouse_init(const char *device)
{
   int fd = open(device, O_RDWR);
  if (fd < 0) {
    printf("Error open mouse:%s\n", strerror(errno));
    exit(EXIT_FAILURE);
  }
  return fd;
}

void mouse_close(int fd)
{
  close(fd);
}
#ifdef TEST_MOUSE
int main(void) {
  int fd = mouse_init("/dev/input/event6");
  for (int i=0; i<5; i++) {
   mouse_move_relative(fd, 100, 100);
   sleep(1);// wait
  }
  mouse_close(fd);
  return 0;
}
#endif
