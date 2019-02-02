/*
 * XWiimote - tools - xwiishow
 * Written 2010-2013 by David Herrmann
 * Dedicated to the Public Domain
 */

/*
 * Interactive Wiimote Testing Tool
 * If you run this tool without arguments, then it shows usage information. If
 * you pass "list" as first argument, it lists all connected Wii Remotes.
 * You need to pass one path as argument and the given wiimote is opened and
 * printed to the screen. When wiimote events are received, then the screen is
 * updated correspondingly. You can use the keyboard to control the wiimote.
 */

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <math.h>
#include <poll.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include "xwiimote.h"
#include "mouse.h"

static int mouse_fd = -1;

enum window_mode {
  MODE_ERROR,
  MODE_NORMAL,
  MODE_EXTENDED,
  MODE_NFS,
};

static struct xwii_iface *iface;
static unsigned int mode = MODE_NORMAL;
static bool freeze = false;

/* error messages */

static void mvprintw(int x, int y, const char *format, ...)
{
  va_list list;
  va_start(list, format);
  printf(format, list);
  va_end(list);
}

static void print_info(const char *format, ...)
{
  va_list list;
  char str[58 + 1];

  va_start(list, format);
  vsnprintf(str, sizeof(str), format, list);
  str[sizeof(str) - 1] = 0;
  va_end(list);
 printf("%s\n", str);
}

static void print_error(const char *format, ...)
{
  va_list list;
  char str[58 + 80 + 1];

  va_start(list, format);
  vsnprintf(str, sizeof(str), format, list);
    str[58] = 0;
  va_end(list);

  printf("%s", str);
}

/* key events */

static void key_show(const struct xwii_event *event)
{
  unsigned int code = event->v.key.code;
  bool pressed = event->v.key.state;
  char *str = NULL;

  printf("code: %d pressed: %d\n", code, pressed);

  if (pressed)
    str = "X";
  else
    str = " ";

  if (code == XWII_KEY_LEFT) {
    mvprintw(4, 7, "%s", str);
  } else if (code == XWII_KEY_RIGHT) {
    mvprintw(4, 11, "%s", str);
  } else if (code == XWII_KEY_UP) {
    switch(mode) {   
      case MODE_NORMAL: mouse_send_wheel(mouse_fd, 1); break;
    }
  } else if (code == XWII_KEY_DOWN) {
    switch(mode) {   
       case MODE_NORMAL: mouse_send_wheel(mouse_fd, -1); break;
    }
  } else if (code == XWII_KEY_A) {
    switch(mode) {   
       case MODE_NORMAL: mouse_send_lmb(mouse_fd, pressed); break;
    }
  } else if (code == XWII_KEY_B) {
    if (pressed)
      str = "B";
    mvprintw(10, 13, "%s", str);
  } else if (code == XWII_KEY_HOME) {
    if (pressed)
      str = "HOME+";
    else
      str = "     ";
    mvprintw(13, 7, "%s", str);
  } else if (code == XWII_KEY_MINUS) {
    if (pressed)
      str = "-";
    mvprintw(13, 3, "%s", str);
  } else if (code == XWII_KEY_PLUS) {
    if (pressed)
      str = "+";
    mvprintw(13, 15, "%s", str);
  } else if (code == XWII_KEY_ONE) {
    if (pressed)
      str = "1";
    mvprintw(20, 9, "%s", str);
  } else if (code == XWII_KEY_TWO) {
    if (pressed)
      str = "2";
    mvprintw(21, 9, "%s", str);
  }
}

/* accelerometer events */

static void accel_show_ext_x(double val)
{
  printf("accell_x=%f\n", val);
}

static void accel_show_ext_y(double val)
{

}

static void accel_show_ext_z(double val)
{
}

static void accel_show_ext(const struct xwii_event *event)
{
  double val;

  /* pow(val, 1/4) for smoother interpolation around the origin */

  val = event->v.abs[0].x;
  val /= 512;
  if (val >= 0)
    val = 10 * pow(val, 0.25);
  else
    val = -10 * pow(-val, 0.25);
  accel_show_ext_x(val);

  val = event->v.abs[0].z;
  val /= 512;
  if (val >= 0)
    val = 5 * pow(val, 0.25);
  else
    val = -5 * pow(-val, 0.25);
  accel_show_ext_z(val);

  val = event->v.abs[0].y;
  val /= 512;
  if (val >= 0)
    val = 5 * pow(val, 0.25);
  else
    val = -5 * pow(-val, 0.25);
  accel_show_ext_y(val);
}

static void accel_show(const struct xwii_event *event)
{
  float dx = 0.01f * event->v.abs[0].x;
  float dy = 0.01f * event->v.abs[0].y;
  float dz = 0.01f * event->v.abs[0].z;
  //printf("AX=%d AY=%d AZ=%d\n", event->v.abs[0].x, event->v.abs[0].y, event->v.abs[0].z);
  //printf("AX=%f AY=%f AZ=%f\n", dx, dy, dz);
  if(mouse_fd>=0) {
     mouse_move_relative(mouse_fd, 10*dx, 10*dy);
  }
}


/* IR events */

static void ir_show_ext(const struct xwii_event *event)
{
}

static void ir_show(const struct xwii_event *event)
{
}


/* motion plus */

static bool mp_do_refresh;

static void mp_show(const struct xwii_event *event)
{
  static int32_t mp_x, mp_y;
  int32_t x, y, z, factor, i;

  if (mp_do_refresh) {
    xwii_iface_get_mp_normalization(iface, &x, &y, &z, &factor);
    x = event->v.abs[0].x + x;
    y = event->v.abs[0].y + y;
    z = event->v.abs[0].z + z;
    xwii_iface_set_mp_normalization(iface, x, y, z, factor);
  }

  x = event->v.abs[0].x;
  y = event->v.abs[0].y;
  z = event->v.abs[0].z;

  //printf("x=%d y=%d z=%d\n", x, y, z);


  /* use x value unchanged for X-direction */
  mp_x += x / 100;
  mp_x = (mp_x < 0) ? 0 : ((mp_x > 10000) ? 10000 : mp_x);
  /* use z value unchanged for Z-direction */
  mp_y += z / 100;
  mp_y = (mp_y < 0) ? 0 : ((mp_y > 10000) ? 10000 : mp_y);

  x = mp_x * 22 / 10000;
  x = (x < 0) ? 0 : ((x > 22) ? 22 : x);
  y = mp_y * 7 / 10000;
  y = (y < 0) ? 0 : ((y > 7) ? 7 : y);
  //printf("x=%d y=%d z=%d\n", x, y, z);
}


static void mp_refresh(void)
{
  mp_do_refresh = true;
}

/* nunchuk */

static void nunchuk_show_ext_x(double val)
{
}

static void nunchuk_show_ext_y(double val)
{

}

static void nunchuk_show_ext_z(double val)
{
}

static void nunchuk_show_ext(const struct xwii_event *event)
{
  double val;
  const char *str = " ";
  int32_t v;

  if (event->type == XWII_EVENT_NUNCHUK_MOVE) {
    /* pow(val, 1/4) for smoother interpolation around the origin */

    val = event->v.abs[1].x;
    val /= 512;
    if (val >= 0)
      val = 10 * pow(val, 0.25);
    else
      val = -10 * pow(-val, 0.25);
    nunchuk_show_ext_x(val);

    val = event->v.abs[1].z;
    val /= 512;
    if (val >= 0)
      val = 5 * pow(val, 0.25);
    else
      val = -5 * pow(-val, 0.25);
    nunchuk_show_ext_z(val);

    val = event->v.abs[1].y;
    val /= 512;
    if (val >= 0)
      val = 5 * pow(val, 0.25);
    else
      val = -5 * pow(-val, 0.25);
    nunchuk_show_ext_y(val);

    v = event->v.abs[0].x * 12;
  }

  if (event->type == XWII_EVENT_NUNCHUK_KEY) {
    if (event->v.key.code == XWII_KEY_C) {
      if (event->v.key.state)
        str = "C";
      mvprintw(37, 6, "%s", str);
    } else if (event->v.key.code == XWII_KEY_Z) {
      if (event->v.key.state)
        str = "Z";
      mvprintw(37, 18, "%s", str);
    }
  }
}


/* balance board */

static void bboard_show_ext(const struct xwii_event *event)
{
  uint16_t w, x, y, z;

  w = event->v.abs[0].x;
  x = event->v.abs[1].x;
  y = event->v.abs[2].x;
  z = event->v.abs[3].x;

  mvprintw(17, 85, " %5d", y);
  mvprintw(17, 96, " %5d", w);
  mvprintw(20, 85, " %5d", z);
  mvprintw(20, 96, " %5d", x);
  mvprintw(13, 86, " %5d", w + x + y + z);
}

/* pro controller */

static void pro_show_ext(const struct xwii_event *event)
{
  uint16_t code = event->v.key.code;
  int32_t v;
  bool pressed = event->v.key.state;
  char *str = NULL;

  if (event->type == XWII_EVENT_PRO_CONTROLLER_MOVE) {
    v = event->v.abs[0].x;
    mvprintw(14, 116, "%5d", v);
    if (v > 1000) {
      mvprintw(16, 118, "     ");
      mvprintw(16, 124, "#####");
    } else if (v > 800) {
      mvprintw(16, 118, "     ");
      mvprintw(16, 124, "#### ");
    } else if (v > 600) {
      mvprintw(16, 118, "     ");
      mvprintw(16, 124, "###  ");
    } else if (v > 400) {
      mvprintw(16, 118, "     ");
      mvprintw(16, 124, "##   ");
    } else if (v > 200) {
      mvprintw(16, 118, "     ");
      mvprintw(16, 124, "#    ");
    } else if (v > -200) {
      mvprintw(16, 118, "     ");
      mvprintw(16, 124, "     ");
    } else if (v > -400) {
      mvprintw(16, 118, "    #");
      mvprintw(16, 124, "     ");
    } else if (v > -600) {
      mvprintw(16, 118, "   ##");
      mvprintw(16, 124, "     ");
    } else if (v > -800) {
      mvprintw(16, 118, "  ###");
      mvprintw(16, 124, "     ");
    } else if (v > -1000) {
      mvprintw(16, 118, " ####");
      mvprintw(16, 124, "     ");
    } else {
      mvprintw(16, 118, "#####");
      mvprintw(16, 124, "     ");
    }

    v = -event->v.abs[0].y;
    mvprintw(14, 125, "%5d", v);
    if (v > 1000) {
      mvprintw(14, 123, "#");
      mvprintw(15, 123, "#");
      mvprintw(17, 123, " ");
      mvprintw(18, 123, " ");
    } else if (v > 200) {
      mvprintw(14, 123, " ");
      mvprintw(15, 123, "#");
      mvprintw(17, 123, " ");
      mvprintw(18, 123, " ");
    } else if (v > -200) {
      mvprintw(14, 123, " ");
      mvprintw(15, 123, " ");
      mvprintw(17, 123, " ");
      mvprintw(18, 123, " ");
    } else if (v > -1000) {
      mvprintw(14, 123, " ");
      mvprintw(15, 123, " ");
      mvprintw(17, 123, "#");
      mvprintw(18, 123, " ");
    } else {
      mvprintw(14, 123, " ");
      mvprintw(15, 123, " ");
      mvprintw(17, 123, "#");
      mvprintw(18, 123, "#");
    }

    v = event->v.abs[1].x;
    mvprintw(14, 134, "%5d", v);
    if (v > 1000) {
      mvprintw(16, 136, "     ");
      mvprintw(16, 142, "#####");
    } else if (v > 800) {
      mvprintw(16, 136, "     ");
      mvprintw(16, 142, "#### ");
    } else if (v > 600) {
      mvprintw(16, 136, "     ");
      mvprintw(16, 142, "###  ");
    } else if (v > 400) {
      mvprintw(16, 136, "     ");
      mvprintw(16, 142, "##   ");
    } else if (v > 200) {
      mvprintw(16, 136, "     ");
      mvprintw(16, 142, "#    ");
    } else if (v > -200) {
      mvprintw(16, 136, "     ");
      mvprintw(16, 142, "     ");
    } else if (v > -400) {
      mvprintw(16, 136, "    #");
      mvprintw(16, 142, "     ");
    } else if (v > -600) {
      mvprintw(16, 136, "   ##");
      mvprintw(16, 142, "     ");
    } else if (v > -800) {
      mvprintw(16, 136, "  ###");
      mvprintw(16, 142, "     ");
    } else if (v > -1000) {
      mvprintw(16, 136, " ####");
      mvprintw(16, 142, "     ");
    } else {
      mvprintw(16, 136, "#####");
      mvprintw(16, 142, "     ");
    }

    v = -event->v.abs[1].y;
    mvprintw(14, 143, "%5d", v);
    if (v > 1000) {
      mvprintw(14, 141, "#");
      mvprintw(15, 141, "#");
      mvprintw(17, 141, " ");
      mvprintw(18, 141, " ");
    } else if (v > 200) {
      mvprintw(14, 141, " ");
      mvprintw(15, 141, "#");
      mvprintw(17, 141, " ");
      mvprintw(18, 141, " ");
    } else if (v > -200) {
      mvprintw(14, 141, " ");
      mvprintw(15, 141, " ");
      mvprintw(17, 141, " ");
      mvprintw(18, 141, " ");
    } else if (v > -1000) {
      mvprintw(14, 141, " ");
      mvprintw(15, 141, " ");
      mvprintw(17, 141, "#");
      mvprintw(18, 141, " ");
    } else {
      mvprintw(14, 141, " ");
      mvprintw(15, 141, " ");
      mvprintw(17, 141, "#");
      mvprintw(18, 141, "#");
    }
  } else if (event->type == XWII_EVENT_PRO_CONTROLLER_KEY) {
    if (pressed)
      str = "X";
    else
      str = " ";

    if (code == XWII_KEY_A) {
      if (pressed)
        str = "A";
      mvprintw(20, 156, "%s", str);
    } else if (code == XWII_KEY_B) {
      if (pressed)
        str = "B";
      mvprintw(21, 154, "%s", str);
    } else if (code == XWII_KEY_X) {
      if (pressed)
        str = "X";
      mvprintw(19, 154, "%s", str);
    } else if (code == XWII_KEY_Y) {
      if (pressed)
        str = "Y";
      mvprintw(20, 152, "%s", str);
    } else if (code == XWII_KEY_PLUS) {
      if (pressed)
        str = "+";
      mvprintw(21, 142, "%s", str);
    } else if (code == XWII_KEY_MINUS) {
      if (pressed)
        str = "-";
      mvprintw(21, 122, "%s", str);
    } else if (code == XWII_KEY_HOME) {
      if (pressed)
        str = "HOME+";
      else
        str = "     ";
      mvprintw(21, 130, "%s", str);
    } else if (code == XWII_KEY_LEFT) {
      mvprintw(18, 108, "%s", str);
    } else if (code == XWII_KEY_RIGHT) {
      mvprintw(18, 112, "%s", str);
    } else if (code == XWII_KEY_UP) {
      mvprintw(16, 110, "%s", str);
    } else if (code == XWII_KEY_DOWN) {
      mvprintw(20, 110, "%s", str);
    } else if (code == XWII_KEY_TL) {
      if (pressed)
        str = "TL";
      else
        str = "  ";
      mvprintw(14, 108, "%s", str);
    } else if (code == XWII_KEY_TR) {
      if (pressed)
        str = "TR";
      else
        str = "  ";
      mvprintw(14, 155, "%s", str);
    } else if (code == XWII_KEY_ZL) {
      if (pressed)
        str = "ZL";
      else
        str = "  ";
      mvprintw(13, 108, "%s", str);
    } else if (code == XWII_KEY_ZR) {
      if (pressed)
        str = "ZR";
      else
        str = "  ";
      mvprintw(13, 155, "%s", str);
    } else if (code == XWII_KEY_THUMBL) {
      if (!pressed)
        str = "+";
      mvprintw(16, 123, "%s", str);
    } else if (code == XWII_KEY_THUMBR) {
      if (!pressed)
        str = "+";
      mvprintw(16, 141, "%s", str);
    }
  }
}

/* classic controller */

static void classic_show_ext(const struct xwii_event *event)
{
  struct xwii_event ev;
  int32_t v;
  const char *str;

  /* forward key events to pro handler */
  if (event->type == XWII_EVENT_CLASSIC_CONTROLLER_KEY) {
    ev = *event;
    ev.type = XWII_EVENT_PRO_CONTROLLER_KEY;
    return pro_show_ext(&ev);
  }

  /* forward axis events to pro handler... */
  if (event->type == XWII_EVENT_CLASSIC_CONTROLLER_MOVE) {
    ev = *event;
    ev.type = XWII_EVENT_PRO_CONTROLLER_MOVE;
    ev.v.abs[0].x *= 45;
    ev.v.abs[0].y *= 45;
    ev.v.abs[1].x *= 45;
    ev.v.abs[1].y *= 45;
    pro_show_ext(&ev);

    /* ...but handle RT/LT triggers which are not reported by pro
     * controllers. Note that if they report MAX (31) a key event is
     * sent, too. */
    v = event->v.abs[2].x;
    if (v < 8)
      str = "  ";
    else if (v < 16)
      str = "--";
    else if (v < 24)
      str = "++";
    else if (v < 32)
      str = "**";
    else if (v < 48)
      str = "##";
    else
      str = "TL";
    mvprintw(14, 108, "%s", str);

    v = event->v.abs[2].y;
    if (v < 8)
      str = "  ";
    else if (v < 16)
      str = "--";
    else if (v < 24)
      str = "++";
    else if (v < 32)
      str = "**";
    else if (v < 48)
      str = "##";
    else
      str = "TL";
    mvprintw(14, 155, "%s", str);
  }
}


/* guitar */
static void guit_show_ext(const struct xwii_event *event)
{
  uint16_t code = event->v.key.code;
  bool pressed = event->v.key.state;
  int32_t v;

  if (event->type == XWII_EVENT_GUITAR_MOVE) {
    v = event->v.abs[1].x;
    switch (v) {
    case 0:
      mvprintw(33, 86, "         ___ ");
      break;
    case 1:
      mvprintw(33, 86, ">        ___ ");
      break;
    case 2:
      mvprintw(33, 86, ">>       ___ ");
      break;
    case 3:
      mvprintw(33, 86, ">>>      ___ ");
      break;
    case 4:
      mvprintw(33, 86, ">>>>     ___ ");
      break;
    case 5:
      mvprintw(33, 86, ">>>>>    ___ ");
      break;
    case 6:
      mvprintw(33, 86, ">>>>>>   ___ ");
      break;
    case 7:
      mvprintw(33, 86, ">>>>>>>  ___ ");
      break;
    case 8:
      mvprintw(33, 86, ">>>>>>>> ___ ");
      break;
    case 9:
      mvprintw(33, 86, ">>>>>>>>>___ ");
      break;
    case 10:
      mvprintw(33, 86, ">>>>>>>>>>__ ");
      break;
    case 11:
      mvprintw(33, 86, ">>>>>>>>>>>_ ");
      break;
    case 12:
      mvprintw(33, 86, ">>>>>>>>>>>> ");
      break;
    case 13:
      mvprintw(33, 86, ">>>>>>>>>>>>>");
      break;
    }

    v = event->v.abs[0].x;
    mvprintw(38, 84, "%3d", v);
    if (v > 25) {
      mvprintw(40, 84, "     ");
      mvprintw(40, 90, "#####");
    } else if (v > 20) {
      mvprintw(40, 84, "     ");
      mvprintw(40, 90, "#### ");
    } else if (v > 15) {
      mvprintw(40, 84, "     ");
      mvprintw(40, 90, "###  ");
    } else if (v > 10) {
      mvprintw(40, 84, "     ");
      mvprintw(40, 90, "##   ");
    } else if (v > 5) {
      mvprintw(40, 84, "     ");
      mvprintw(40, 90, "#    ");
    } else if (v > -5) {
      mvprintw(40, 84, "     ");
      mvprintw(40, 90, "     ");
    } else if (v > -10) {
      mvprintw(40, 84, "    #");
      mvprintw(40, 90, "     ");
    } else if (v > -15) {
      mvprintw(40, 84, "   ##");
      mvprintw(40, 90, "     ");
    } else if (v > -20) {
      mvprintw(40, 84, "  ###");
      mvprintw(40, 90, "     ");
    } else if (v > -25) {
      mvprintw(40, 84, " ####");
      mvprintw(40, 90, "     ");
    } else {
      mvprintw(40, 84, "#####");
      mvprintw(40, 90, "     ");
    }

    v = event->v.abs[0].y;
    mvprintw(38, 93, "%3d", v);
    if (v > 20) {
      mvprintw(38, 89, "#");
      mvprintw(39, 89, "#");
      mvprintw(41, 89, " ");
      mvprintw(42, 89, " ");
    } else if (v > 10) {
      mvprintw(38, 89, " ");
      mvprintw(39, 89, "#");
      mvprintw(41, 89, " ");
      mvprintw(42, 89, " ");
    } else if (v > -10) {
      mvprintw(38, 89, " ");
      mvprintw(39, 89, " ");
      mvprintw(41, 89, " ");
      mvprintw(42, 89, " ");
    } else if (v > -20) {
      mvprintw(38, 89, " ");
      mvprintw(39, 89, " ");
      mvprintw(41, 89, "#");
      mvprintw(42, 89, " ");
    } else {
      mvprintw(38, 89, " ");
      mvprintw(39, 89, " ");
      mvprintw(41, 89, "#");
      mvprintw(42, 89, "#");
    }

  } else if (event->type == XWII_EVENT_GUITAR_KEY) {
    switch (code) {
    case XWII_KEY_FRET_FAR_UP:
      if (pressed) {
        mvprintw(30, 141, "X");
        mvprintw(31, 141, "X");
      } else {
        mvprintw(30, 141, " ");
        mvprintw(31, 141, "_");
      }
      break;
    case XWII_KEY_FRET_UP:
      if (pressed) {
        mvprintw(30, 137, "X");
        mvprintw(31, 137, "X");
      } else {
        mvprintw(30, 137, " ");
        mvprintw(31, 137, "_");
      }
      break;
    case XWII_KEY_FRET_MID:
      if (pressed) {
        mvprintw(30, 133, "X");
        mvprintw(31, 133, "X");
      } else {
        mvprintw(30, 133, " ");
        mvprintw(31, 133, "_");
      }
      break;
    case XWII_KEY_FRET_LOW:
      if (pressed) {
        mvprintw(30, 129, "X");
        mvprintw(31, 129, "X");
      } else {
        mvprintw(30, 129, " ");
        mvprintw(31, 129, "_");
      }
      break;
    case XWII_KEY_FRET_FAR_LOW:
      if (pressed) {
        mvprintw(30, 125, "X");
        mvprintw(31, 125, "X");
      } else {
        mvprintw(30, 125, " ");
        mvprintw(31, 125, "_");
      }
      break;
    case XWII_KEY_STRUM_BAR_UP:
      if (pressed)
        mvprintw(30, 98, "---------");
      else
        mvprintw(30, 98, "_________");
      break;
    case XWII_KEY_STRUM_BAR_DOWN:
      if (pressed) {
        mvprintw(29, 97, "          ");
        mvprintw(30, 97, " _________  ");
        mvprintw(31, 98, "\\--------\\");
      } else {
        mvprintw(29, 97, "__________");
        mvprintw(30, 97, "\\_________\\");
        mvprintw(31, 98, "          ");
      }
      break;
    case XWII_KEY_HOME:
      if (pressed) {
        mvprintw(29, 89, "X");
        mvprintw(30, 89, "X");
      } else {
        mvprintw(29, 89, " ");
        mvprintw(30, 89, "_");
      }
      break;
    case XWII_KEY_PLUS:
      if (pressed) {
        mvprintw(28, 89, "+");
        mvprintw(31, 89, "+");
      } else {
        mvprintw(28, 89, "_");
        mvprintw(31, 89, "_");
      }
      break;
    }
  }
}

/* guitar hero drums */
static void drums_show_ext(const struct xwii_event *event)
{
  uint16_t code = event->v.key.code;
  bool pressed = event->v.key.state;
  int32_t v;
  int i, j, n;

  if (event->type == XWII_EVENT_DRUMS_KEY) {
    switch (code) {
    case XWII_KEY_MINUS:
      if (pressed)
        mvprintw(45, 147, "-");
      else
        mvprintw(45, 147, " ");
      break;
    case XWII_KEY_PLUS:
      if (pressed)
        mvprintw(45, 153, "+");
      else
        mvprintw(45, 153, " ");
      break;
    default:
      break;
    }
  }

  if (event->type != XWII_EVENT_DRUMS_MOVE)
    return;

  v = event->v.abs[XWII_DRUMS_ABS_PAD].x;
  mvprintw(38, 145, "%3d", v);
  if (v > 25) {
    mvprintw(40, 145, "     ");
    mvprintw(40, 151, "#####");
  } else if (v > 20) {
    mvprintw(40, 145, "     ");
    mvprintw(40, 151, "#### ");
  } else if (v > 15) {
    mvprintw(40, 145, "     ");
    mvprintw(40, 151, "###  ");
  } else if (v > 10) {
    mvprintw(40, 145, "     ");
    mvprintw(40, 151, "##   ");
  } else if (v > 5) {
    mvprintw(40, 145, "     ");
    mvprintw(40, 151, "#    ");
  } else if (v > -5) {
    mvprintw(40, 145, "     ");
    mvprintw(40, 151, "     ");
  } else if (v > -10) {
    mvprintw(40, 145, "    #");
    mvprintw(40, 151, "     ");
  } else if (v > -15) {
    mvprintw(40, 145, "   ##");
    mvprintw(40, 151, "     ");
  } else if (v > -20) {
    mvprintw(40, 145, "  ###");
    mvprintw(40, 151, "     ");
  } else if (v > -25) {
    mvprintw(40, 145, " ####");
    mvprintw(40, 151, "     ");
  } else {
    mvprintw(40, 145, "#####");
    mvprintw(40, 151, "     ");
  }

  v = event->v.abs[XWII_DRUMS_ABS_PAD].y;
  mvprintw(38, 154, "%3d", v);
  if (v > 20) {
    mvprintw(38, 150, "#");
    mvprintw(39, 150, "#");
    mvprintw(41, 150, " ");
    mvprintw(42, 150, " ");
  } else if (v > 10) {
    mvprintw(38, 150, " ");
    mvprintw(39, 150, "#");
    mvprintw(41, 150, " ");
    mvprintw(42, 150, " ");
  } else if (v > -10) {
    mvprintw(38, 150, " ");
    mvprintw(39, 150, " ");
    mvprintw(41, 150, " ");
    mvprintw(42, 150, " ");
  } else if (v > -20) {
    mvprintw(38, 150, " ");
    mvprintw(39, 150, " ");
    mvprintw(41, 150, "#");
    mvprintw(42, 150, " ");
  } else {
    mvprintw(38, 150, " ");
    mvprintw(39, 150, " ");
    mvprintw(41, 150, "#");
    mvprintw(42, 150, "#");
  }

  for (n = 0; n < XWII_DRUMS_ABS_NUM; ++n) {
    if (n == XWII_DRUMS_ABS_BASS) {
      v = event->v.abs[n].x;
      switch (v) {
      case 0:
        mvprintw(44, 100, "   ");
        break;
      case 1:
        mvprintw(44, 100, " . ");
        break;
      case 2:
        mvprintw(44, 100, "...");
        break;
      case 3:
        mvprintw(44, 100, ".+.");
        break;
      case 4:
        mvprintw(44, 100, "+++");
        break;
      case 5:
        mvprintw(44, 100, "+#+");
        break;
      case 6:
        mvprintw(44, 100, "*#*");
        break;
      case 7:
        mvprintw(44, 100, "###");
        break;
      }
      mvprintw(45, 100, "<%1d>", v);
    } else {
      i = j = 0;
      switch (n) {
      case XWII_DRUMS_ABS_CYMBAL_RIGHT:
        i = 35;
        j = 125;
        break;
      case XWII_DRUMS_ABS_TOM_LEFT:
        i = 41;
        j = 107;
        break;
      case XWII_DRUMS_ABS_CYMBAL_LEFT:
        i = 35;
        j = 113;
        break;
      case XWII_DRUMS_ABS_TOM_FAR_RIGHT:
        i = 41;
        j = 131;
        break;
      case XWII_DRUMS_ABS_TOM_RIGHT:
        i = 41;
        j = 119;
        break;
      }

      switch(n) {
      case XWII_DRUMS_ABS_CYMBAL_RIGHT:
      case XWII_DRUMS_ABS_TOM_LEFT:
      case XWII_DRUMS_ABS_CYMBAL_LEFT:
      case XWII_DRUMS_ABS_TOM_FAR_RIGHT:
      case XWII_DRUMS_ABS_TOM_RIGHT:
        v = event->v.abs[n].x;
        switch(v) {
        case 0:
          mvprintw(i,   j, "  ______  ");
          mvprintw(i+1, j, " /      \\ ");
          mvprintw(i+2, j, "/        \\");
          mvprintw(i+3, j, "|   ++   |");
          mvprintw(i+4, j, "\\        /");
          mvprintw(i+5, j, " \\______/ ");
          break;
        case 1:
          mvprintw(i,   j, "  ______  ");
          mvprintw(i+1, j, " /      \\ ");
          mvprintw(i+2, j, "/   ..   \\");
          mvprintw(i+3, j, "|  .+1.  |");
          mvprintw(i+4, j, "\\   ..   /");
          mvprintw(i+5, j, " \\______/ ");
          break;
        case 2:
          mvprintw(i,   j, "  ______  ");
          mvprintw(i+1, j, " /      \\ ");
          mvprintw(i+2, j, "/  ....  \\");
          mvprintw(i+3, j, "|  .+2.  |");
          mvprintw(i+4, j, "\\  ....  /");
          mvprintw(i+5, j, " \\______/ ");
          break;
        case 3:
          mvprintw(i,   j, "  ______  ");
          mvprintw(i+1, j, " /      \\ ");
          mvprintw(i+2, j, "/ ...... \\");
          mvprintw(i+3, j, "| ..+3.. |");
          mvprintw(i+4, j, "\\ ...... /");
          mvprintw(i+5, j, " \\______/ ");
          break;
        case 4:
          mvprintw(i,   j, "  ______  ");
          mvprintw(i+1, j, " /      \\ ");
          mvprintw(i+2, j, "/........\\");
          mvprintw(i+3, j, "|...+4...|");
          mvprintw(i+4, j, "\\......../");
          mvprintw(i+5, j, " \\______/ ");
          break;
        case 5:
          mvprintw(i,   j, "  ______  ");
          mvprintw(i+1, j, " /  ..  \\ ");
          mvprintw(i+2, j, "/........\\");
          mvprintw(i+3, j, "|...+5...|");
          mvprintw(i+4, j, "\\......../");
          mvprintw(i+5, j, " \\______/ ");
          break;
        case 6:
          mvprintw(i,   j, "  ______  ");
          mvprintw(i+1, j, " / .... \\ ");
          mvprintw(i+2, j, "/........\\");
          mvprintw(i+3, j, "|...+6...|");
          mvprintw(i+4, j, "\\......../");
          mvprintw(i+5, j, " \\_...._/ ");
          break;
        case 7:
          mvprintw(i,   j, "  ______  ");
          mvprintw(i+1, j, " /......\\ ");
          mvprintw(i+2, j, "/........\\");
          mvprintw(i+3, j, "|...+7...|");
          mvprintw(i+4, j, "\\......../");
          mvprintw(i+5, j, " \\....../ ");
          break;
        }
      }
    }
  }
}


/* rumble events */

/* LEDs */

static bool led_state[4];

static void led_show(int n, bool on)
{
  mvprintw(5, 59 + n*5, on ? "(#%i)" : " -%i ", n+1);
}

static void led_refresh(int n)
{
  int ret;

  ret = xwii_iface_get_led(iface, XWII_LED(n+1), &led_state[n]);
  if (ret)
    print_error("Error: Cannot read LED state");
  else
    led_show(n, led_state[n]);
}

/* battery status */

static void battery_show(uint8_t capacity)
{
  int i;

  mvprintw(7, 29, "%3u%%", capacity);

  mvprintw(7, 35, "          ");
  for (i = 0; i * 10 < capacity; ++i)
    mvprintw(7, 35 + i, "#");
}

static void battery_refresh(void)
{
  int ret;
  uint8_t capacity;

  ret = xwii_iface_get_battery(iface, &capacity);
  if (ret)
    print_error("Error: Cannot read battery capacity");
  else
    battery_show(capacity);
}

/* device type */

static void devtype_refresh(void)
{
  int ret;
  char *name;

  ret = xwii_iface_get_devtype(iface, &name);
  if (ret) {
    print_error("Error: Cannot read device type");
  } else {
    mvprintw(9, 28, "                                                   ");
    mvprintw(9, 28, "%s", name);
    free(name);
  }
}

/* extension type */

static void extension_refresh(void)
{
  int ret;
  char *name;

  ret = xwii_iface_get_extension(iface, &name);
  if (ret) {
    print_error("Error: Cannot read extension type");
  } else {
    mvprintw(7, 54, "                      ");
    mvprintw(7, 54, "%s", name);
    free(name);
  }

  if (xwii_iface_available(iface) & XWII_IFACE_MOTION_PLUS)
    mvprintw(7, 77, "M+");
  else
    mvprintw(7, 77, "  ");
}

/* basic window setup */

static void refresh_all(void)
{
  battery_refresh();
  led_refresh(0);
  led_refresh(1);
  led_refresh(2);
  led_refresh(3);
  devtype_refresh();
  extension_refresh();
  mp_refresh();

  if (geteuid() != 0)
    mvprintw(20, 22, "Warning: Please run as root! (sysfs+evdev access needed)");
}


/* device watch events */

static void handle_watch(void)
{
  static unsigned int num;
  int ret;

  print_info("Info: Watch Event #%u", ++num);

  ret = xwii_iface_open(iface, xwii_iface_available(iface) |
             XWII_IFACE_WRITABLE);
  if (ret)
    print_error("Error: Cannot open interface: %d", ret);

  refresh_all();
}

/* keyboard handling */


static int run_iface(struct xwii_iface *iface)
{
  struct xwii_event event;
  int ret = 0, fds_num;
  struct pollfd fds[2];

  memset(fds, 0, sizeof(fds));
  fds[0].fd = 0;
  fds[0].events = POLLIN;
  fds[1].fd = xwii_iface_get_fd(iface);
  fds[1].events = POLLIN;
  fds_num = 2;

  ret = xwii_iface_watch(iface, true);
  if (ret)
    print_error("Error: Cannot initialize hotplug watch descriptor");

  while (true) {
    ret = poll(fds, fds_num, -1);
    if (ret < 0) {
      if (errno != EINTR) {
        ret = -errno;
        print_error("Error: Cannot poll fds: %d", ret);
        break;
      }
    }

    ret = xwii_iface_dispatch(iface, &event, sizeof(event));
    if (ret) {
      if (ret != -EAGAIN) {
        print_error("Error: Read failed with err:%d",
              ret);
        break;
      }
    } else if (!freeze) {
      //printf("event.type: %d\n", event.type);
      switch (event.type) {
      case XWII_EVENT_GONE:
        print_info("Info: Device gone");
        fds[1].fd = -1;
        fds[1].events = 0;
        fds_num = 1;
        break;
      case XWII_EVENT_WATCH:
        handle_watch();
        break;
      case XWII_EVENT_KEY:
        if (mode != MODE_ERROR) {
        printf("event key\n");
          key_show(&event);
        }
        break;
      case XWII_EVENT_ACCEL:
        if (mode == MODE_EXTENDED)
          accel_show_ext(&event);
        if (mode != MODE_ERROR)
          accel_show(&event);
        break;
      case XWII_EVENT_IR:
        if (mode == MODE_EXTENDED)
          ir_show_ext(&event);
        if (mode != MODE_ERROR)
          ir_show(&event);
        break;
      case XWII_EVENT_MOTION_PLUS:
        if (mode != MODE_ERROR)
          mp_show(&event);
        break;
      case XWII_EVENT_NUNCHUK_KEY:
      case XWII_EVENT_NUNCHUK_MOVE:
        if (mode == MODE_EXTENDED)
          nunchuk_show_ext(&event);
        break;
      case XWII_EVENT_CLASSIC_CONTROLLER_KEY:
      case XWII_EVENT_CLASSIC_CONTROLLER_MOVE:
        if (mode == MODE_EXTENDED)
          classic_show_ext(&event);
        break;
      case XWII_EVENT_BALANCE_BOARD:
        if (mode == MODE_EXTENDED)
          bboard_show_ext(&event);
        break;
      case XWII_EVENT_PRO_CONTROLLER_KEY:
      case XWII_EVENT_PRO_CONTROLLER_MOVE:
        if (mode == MODE_EXTENDED)
          pro_show_ext(&event);
        break;
      case XWII_EVENT_GUITAR_KEY:
      case XWII_EVENT_GUITAR_MOVE:
        if (mode == MODE_EXTENDED)
          guit_show_ext(&event);
        break;
      case XWII_EVENT_DRUMS_KEY:
      case XWII_EVENT_DRUMS_MOVE:
        if (mode == MODE_EXTENDED)
          drums_show_ext(&event);
        break;
      }
    }

#if 0
    ret = keyboard();
    if (ret == -ECANCELED)
      return 0;
    else if (ret)
      return ret;
#endif
  }

  return ret;
}

static int enumerate(void)
{
  struct xwii_monitor *mon;
  char *ent;
  int num = 0;

  mon = xwii_monitor_new(false, false);
  if (!mon) {
    printf("Cannot create monitor\n");
    return -EINVAL;
  }

  while ((ent = xwii_monitor_poll(mon))) {
    printf("  Found device #%d: %s\n", ++num, ent);
    free(ent);
  }

  xwii_monitor_unref(mon);
  return 0;
}

static char *get_dev(int num)
{
  struct xwii_monitor *mon;
  char *ent;
  int i = 0;

  mon = xwii_monitor_new(false, false);
  if (!mon) {
    printf("Cannot create monitor\n");
    return NULL;
  }

  while ((ent = xwii_monitor_poll(mon))) {
    if (++i == num)
      break;
    free(ent);
  }

  xwii_monitor_unref(mon);

  if (!ent)
    printf("Cannot find device with number #%d\n", num);

  return ent;
}

static void free_mouse(void)
{
  mouse_close(mouse_fd);
}

int main(int argc, char **argv)
{
  int ret = 0;
  char *path = NULL;

  if (argc < 2 || !strcmp(argv[1], "-h")) {
    printf("Usage:\n");
    printf("\txwiishow [-h]: Show help\n");
    printf("\txwiishow list: List connected devices\n");
    printf("\txwiishow <num>: Show device with number #num\n");
    printf("\txwiishow /sys/path/to/device: Show given device\n");
    printf("UI commands:\n");
    printf("\tq: Quit application\n");
    printf("\tf: Freeze/Unfreeze screen\n");
    printf("\ts: Refresh static values (like battery or calibration)\n");
    printf("\tk: Toggle key events\n");
    printf("\tr: Toggle rumble motor\n");
    printf("\ta: Toggle accelerometer\n");
    printf("\ti: Toggle IR camera\n");
    printf("\tm: Toggle motion plus\n");
    printf("\tn: Toggle normalization for motion plus\n");
    printf("\tb: Toggle balance board\n");
    printf("\tp: Toggle pro controller\n");
    printf("\tg: Toggle guitar controller\n");
    printf("\td: Toggle drums controller\n");
    printf("\t1: Toggle LED 1\n");
    printf("\t2: Toggle LED 2\n");
    printf("\t3: Toggle LED 3\n");
    printf("\t4: Toggle LED 4\n");
    ret = -1;
  } else if (!strcmp(argv[1], "list")) {
    printf("Listing connected Wii Remote devices:\n");
    ret = enumerate();
    printf("End of device list\n");
  } else {
    if(argc<3) {
      fprintf(stderr, "Usage: [sudo] %s <wii_device> <mouse_input_device> [mode]\nExample: sudo %s 1 /dev/input/event6\n         sudo %s 1 /dev/input/event6 nfs\n", argv[0], argv[0], argv[0]);
      exit(EXIT_FAILURE);
    }
   
    if(argc==4) {
      if(!strcmp(argv[3], "nfs")) mode = MODE_NFS;
    }

    mouse_fd = mouse_init(argv[2]);
    atexit(free_mouse);
    if (argv[1][0] != '/')
      path = get_dev(atoi(argv[1]));
      
    ret = xwii_iface_new(&iface, path ? path : argv[1]);
    free(path);
    if (ret) {
      printf("Cannot create xwii_iface '%s' err:%d\n",
                argv[1], ret);
    } else {

      ret = xwii_iface_open(iface,
                xwii_iface_available(iface) |
                XWII_IFACE_WRITABLE);
      if (ret)
        print_error("Error: Cannot open interface: %d",
              ret);

      ret = run_iface(iface);
      xwii_iface_unref(iface);
      if (ret) {
        print_error("Program failed; press any key to exit");
      }
    }
  }

  return abs(ret);
}
