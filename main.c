#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#define W 250
#define H 80

static const char *msg[] = {
    "Press 'r' to reboot, 's' to shutdown", "Press 'r' again to confirm",
    "Press 's' again to confirm", "Rebooting...", "Shutting down..."};

static void exec_cmd(const char *cmd) {
  if (!fork()) {
    execlp("loginctl", "loginctl", cmd, (char *)NULL);
    _exit(127);
  }
}

int main(void) {
  Display *d = XOpenDisplay(NULL);
  if (!d)
    return 1;

  signal(SIGCHLD, SIG_IGN);

  int s = DefaultScreen(d);
  Window w = XCreateSimpleWindow(d, RootWindow(d, s), 100, 100, W, H, 1,
                                 BlackPixel(d, s), 0x222222);

  XStoreName(d, w, "minipm");

  XClassHint *classHint = XAllocClassHint();
  if (classHint) {
    classHint->res_name = "minipm";
    classHint->res_class = "minipm";
    XSetClassHint(d, w, classHint);
    XFree(classHint);
  }
  XSelectInput(d, w, ExposureMask | KeyPressMask);

  Atom wm_del = XInternAtom(d, "WM_DELETE_WINDOW", 0);
  XSetWMProtocols(d, w, &wm_del, 1);

  GC gc = XCreateGC(d, w, 0, NULL);
  XMapWindow(d, w);

  int state = 0; // 0=normal, 1=reboot confirm, 2=shutdown confirm

  XEvent e;
  while (1) {
    XNextEvent(d, &e);

    if (e.type == Expose) {
      XSetForeground(d, gc, 0x222222);
      XFillRectangle(d, w, gc, 0, 0, W, H);
      XSetForeground(d, gc, 0xFFFFFF);

      const char *text = msg[state];
      int len = strlen(text);
      XDrawString(d, w, gc, (W - len * 6) / 2, H / 2, text, len);

    } else if (e.type == KeyPress) {
      KeySym k = XLookupKeysym(&e.xkey, 0);

      if (k == XK_q || k == XK_Escape)
        break;

      if (k == XK_r) {
        state = (state == 1) ? 3 : 1;
        if (state == 3)
          exec_cmd("reboot");

      } else if (k == XK_s) {
        state = (state == 2) ? 4 : 2;
        if (state == 4)
          exec_cmd("poweroff");

      } else if (k == XK_Return) {
        if (state == 1) {
          state = 3;
          exec_cmd("reboot");
        }
        if (state == 2) {
          state = 4;
          exec_cmd("poweroff");
        }

      } else {
        state = 0;
      }

      XClearArea(d, w, 0, 0, 0, 0, True);
    } else if (e.type == ClientMessage && (Atom)e.xclient.data.l[0] == wm_del) {
      break;
    }
  }

  XFreeGC(d, gc);
  XDestroyWindow(d, w);
  XCloseDisplay(d);
  return 0;
}
