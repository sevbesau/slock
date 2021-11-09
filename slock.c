/* See LICENSE file for license details. */
#define _XOPEN_SOURCE 500
#if HAVE_SHADOW_H
#include <shadow.h>
#endif

#include <ctype.h>
#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/Xinerama.h>
#include <X11/extensions/dpms.h>
#include <X11/keysym.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <Imlib2.h>

#include "arg.h"
#include "util.h"

char *argv0;

/* global count to prevent repeated error messages */
int count_error = 0;

enum
{
  ICON,
  INPUT,
  FAILED,
  NUMCOLS
};

struct lock
{
  int screen;
  Window root, win;
  Pixmap pmap;
  Pixmap bgmap;
  unsigned long colors[NUMCOLS];
  XColor xcolors[NUMCOLS];
};

struct xrandr
{
  int active;
  int evbase;
  int errbase;
};

#include "config.h"

Imlib_Image buffer, image, icon;

static void
die(const char *errstr, ...)
{
  va_list ap;

  va_start(ap, errstr);
  vfprintf(stderr, errstr, ap);
  va_end(ap);
  exit(1);
}

#ifdef __linux__
#include <fcntl.h>
#include <linux/oom.h>

static void
dontkillme(void)
{
  FILE *f;
  const char oomfile[] = "/proc/self/oom_score_adj";

  if (!(f = fopen(oomfile, "w")))
  {
    if (errno == ENOENT)
      return;
    die("slock: fopen %s: %s\n", oomfile, strerror(errno));
  }
  fprintf(f, "%d", OOM_SCORE_ADJ_MIN);
  if (fclose(f))
  {
    if (errno == EACCES)
      die("slock: unable to disable OOM killer. "
          "Make sure to suid or sgid slock.\n");
    else
      die("slock: fclose %s: %s\n", oomfile, strerror(errno));
  }
}
#endif

void draw_key_magic(Display *dpy, struct lock **locks, int screen, unsigned int color)
{
  Window win = locks[screen]->win;
  Window root_win;

  int _x, _y;
  unsigned int icon_w, icon_h, screen_w, screen_h, _b, _d;
  XGetGeometry(dpy, win, &root_win, &_x, &_y, &screen_w, &screen_h, &_b, &_d);

  XColor *rgb = &locks[screen]->xcolors[color];

  int red = ((float)rgb->red / 65535) * 255;
  int green = ((float)rgb->green / 65535) * 255;
  int blue = ((float)rgb->blue / 65535) * 255;

  // printf("red: %d, green: %d, blue:%d\t", rgb->red, rgb->green, rgb->blue);
  // printf("red: %d, green: %d, blue:%d\n", red, green, blue);
  locks[screen]->bgmap = XCreatePixmap(dpy, locks[screen]->root, DisplayWidth(dpy, locks[screen]->screen), DisplayHeight(dpy, locks[screen]->screen), DefaultDepth(dpy, locks[screen]->screen));

  // create the context
  imlib_context_set_display(dpy);
  imlib_context_set_drawable(locks[screen]->bgmap);
  imlib_context_set_visual(DefaultVisual(dpy, locks[screen]->screen));
  imlib_context_set_colormap(DefaultColormap(dpy, locks[screen]->screen));

  // draw the background image
  imlib_context_set_image(image);
  imlib_render_image_on_drawable(0, 0);

  // draw the color
  Screen *scr = ScreenOfDisplay(dpy, DefaultScreen(dpy));
  buffer = imlib_create_image(scr->width, scr->height);

  // imlib_context_set_image(buffer);
  imlib_context_set_color(red, green, blue, 255);
  imlib_image_fill_ellipse(screen_w / 2, screen_h / 2, 200, 200);
  imlib_render_image_on_drawable(0, 0);

  // draw the icon
  imlib_context_set_image(icon);
  icon_h = imlib_image_get_width();
  icon_w = imlib_image_get_height();

  imlib_render_image_on_drawable((screen_w - icon_w) / 2, (screen_h - icon_h) / 2);

  XSetWindowBackgroundPixmap(dpy, locks[screen]->win, locks[screen]->bgmap);
}

static const char *
gethash(void)
{
  const char *hash;
  struct passwd *pw;

  /* Check if the current user has a password entry */
  errno = 0;
  if (!(pw = getpwuid(getuid())))
  {
    if (errno)
      die("slock: getpwuid: %s\n", strerror(errno));
    else
      die("slock: cannot retrieve password entry\n");
  }
  hash = pw->pw_passwd;

#if HAVE_SHADOW_H
  if (!strcmp(hash, "x"))
  {
    struct spwd *sp;
    if (!(sp = getspnam(pw->pw_name)))
      die("slock: getspnam: cannot retrieve shadow entry. "
          "Make sure to suid or sgid slock.\n");
    hash = sp->sp_pwdp;
  }
#else
  if (!strcmp(hash, "*"))
  {
#ifdef __OpenBSD__
    if (!(pw = getpwuid_shadow(getuid())))
      die("slock: getpwnam_shadow: cannot retrieve shadow entry. "
          "Make sure to suid or sgid slock.\n");
    hash = pw->pw_passwd;
#else
    die("slock: getpwuid: cannot retrieve shadow entry. "
        "Make sure to suid or sgid slock.\n");
#endif /* __OpenBSD__ */
  }
#endif /* HAVE_SHADOW_H */

  return hash;
}

static void
readpw(Display *dpy, struct xrandr *rr, struct lock **locks, int nscreens,
       const char *hash)
{
  XRRScreenChangeNotifyEvent *rre;
  char buf[32], passwd[256], *inputhash;
  int num, screen, running, failure, oldc;
  unsigned int len, color;
  KeySym ksym;
  XEvent ev;

  len = 0;
  running = 1;
  failure = 0;
  oldc = ICON;

  while (running && !XNextEvent(dpy, &ev))
  {
    if (ev.type == KeyPress)
    {
      explicit_bzero(&buf, sizeof(buf));
      num = XLookupString(&ev.xkey, buf, sizeof(buf), &ksym, 0);
      if (IsKeypadKey(ksym))
      {
        if (ksym == XK_KP_Enter)
          ksym = XK_Return;
        else if (ksym >= XK_KP_0 && ksym <= XK_KP_9)
          ksym = (ksym - XK_KP_0) + XK_0;
      }
      if (IsFunctionKey(ksym) ||
          IsKeypadKey(ksym) ||
          IsMiscFunctionKey(ksym) ||
          IsPFKey(ksym) ||
          IsPrivateKeypadKey(ksym))
        continue;
      switch (ksym)
      {
      case XK_Return:
        passwd[len] = '\0';
        errno = 0;
        if (!(inputhash = crypt(passwd, hash)))
          fprintf(stderr, "slock: crypt: %s\n", strerror(errno));
        else
          running = !!strcmp(inputhash, hash);
        if (running)
        {
          XBell(dpy, 100);
          failure = 1;
        }
        explicit_bzero(&passwd, sizeof(passwd));
        len = 0;
        break;
      case XK_Escape:
        explicit_bzero(&passwd, sizeof(passwd));
        len = 0;
        break;
      case XK_BackSpace:
        if (len)
          passwd[len--] = '\0';
        break;
      default:
        if (num && !iscntrl((int)buf[0]) &&
            (len + num < sizeof(passwd)))
        {
          memcpy(passwd + len, buf, num);
          len += num;
        }

        // Not in failure state after typing
        failure = 0;

        for (screen = 0; screen < nscreens; screen++)
        {
          draw_key_magic(dpy, locks, screen, INPUT);
          XClearWindow(dpy, locks[screen]->win);
        }
        break;
      }
      color = len ? INPUT : ((failure || failonclear) ? FAILED : ICON);
      if (running && oldc != color)
      {
        for (screen = 0; screen < nscreens; screen++)
        {
          draw_key_magic(dpy, locks, screen, color);
          XClearWindow(dpy, locks[screen]->win);
        }
        oldc = color;
      }
    }
    else if (rr->active && ev.type == rr->evbase + RRScreenChangeNotify)
    {
      rre = (XRRScreenChangeNotifyEvent *)&ev;
      for (screen = 0; screen < nscreens; screen++)
      {
        if (locks[screen]->win == rre->window)
        {
          XResizeWindow(dpy, locks[screen]->win,
                        rre->width, rre->height);
          draw_key_magic(dpy, locks, screen, ICON);
          XClearWindow(dpy, locks[screen]->win);
        }
      }
    }
    else
      for (screen = 0; screen < nscreens; screen++)
        XRaiseWindow(dpy, locks[screen]->win);
  }
}

static struct lock *
lockscreen(Display *dpy, struct xrandr *rr, int screen)
{
  char curs[] = {0, 0, 0, 0, 0, 0, 0, 0};
  int i, ptgrab, kbgrab;
  struct lock *lock;
  XColor color, dummy;
  XSetWindowAttributes wa;
  Cursor invisible;

  if (dpy == NULL || screen < 0 || !(lock = malloc(sizeof(struct lock))))
    return NULL;

  lock->screen = screen;
  lock->root = RootWindow(dpy, lock->screen);

  // allocate the colors
  for (i = 0; i < NUMCOLS; i++)
  {
    XAllocNamedColor(dpy, DefaultColormap(dpy, lock->screen),
                     colorname[i], &color, &dummy);
    lock->colors[i] = color.pixel;
    lock->xcolors[i] = color;
  }

  if (image)
  {
    lock->bgmap = XCreatePixmap(dpy, lock->root, DisplayWidth(dpy, lock->screen), DisplayHeight(dpy, lock->screen), DefaultDepth(dpy, lock->screen));
    int icon_w, icon_h, screen_w, screen_h;

    // draw the background image
    imlib_context_set_image(image);
    screen_w = imlib_image_get_width();
    screen_h = imlib_image_get_height();
    imlib_context_set_display(dpy);
    imlib_context_set_visual(DefaultVisual(dpy, lock->screen));
    imlib_context_set_colormap(DefaultColormap(dpy, lock->screen));
    imlib_context_set_drawable(lock->bgmap);
    imlib_render_image_on_drawable(0, 0);

    // load the icon
    icon = imlib_load_image(icon_path);
    imlib_context_set_image(icon);
    icon_w = imlib_image_get_width();
    icon_h = imlib_image_get_height();

    // modify the icon color
    Imlib_Color_Modifier color_modifier = imlib_create_color_modifier();
    imlib_context_set_color_modifier(color_modifier);

    DATA8 red_table[255] = {0};
    DATA8 green_table[255] = {0};
    DATA8 blue_table[255] = {0};
    DATA8 alpha_table[255] = {0};
    imlib_get_color_modifier_tables(red_table, green_table, blue_table, alpha_table);

    // get the rgb values
    XColor rgb = lock[screen].xcolors[ICON];
    // set the rgb values
    for (int i = 0; i < 255; i++)
    {
      red_table[i] = rgb.red;
      green_table[i] = rgb.green;
      blue_table[i] = rgb.blue;
    }
    imlib_set_color_modifier_tables(red_table, green_table, blue_table, alpha_table);
    imlib_apply_color_modifier();
    imlib_free_color_modifier();

    // draw the icon
    imlib_render_image_on_drawable((screen_w - icon_w) / 2, (screen_h - icon_h) / 2);
    // imlib_free_image();
  }

  /* init */
  wa.override_redirect = 1;
  wa.background_pixel = lock->colors[ICON];
  lock->win = XCreateWindow(dpy, lock->root, 0, 0,
                            DisplayWidth(dpy, lock->screen),
                            DisplayHeight(dpy, lock->screen),
                            0, DefaultDepth(dpy, lock->screen),
                            CopyFromParent,
                            DefaultVisual(dpy, lock->screen),
                            CWOverrideRedirect | CWBackPixel, &wa);
  if (lock->bgmap)
    XSetWindowBackgroundPixmap(dpy, lock->win, lock->bgmap);
  lock->pmap = XCreateBitmapFromData(dpy, lock->win, curs, 8, 8);
  invisible = XCreatePixmapCursor(dpy, lock->pmap, lock->pmap,
                                  &color, &color, 0, 0);
  XDefineCursor(dpy, lock->win, invisible);

  /* Try to grab mouse pointer *and* keyboard for 600ms, else fail the lock */
  for (i = 0, ptgrab = kbgrab = -1; i < 6; i++)
  {
    if (ptgrab != GrabSuccess)
    {
      ptgrab = XGrabPointer(dpy, lock->root, False,
                            ButtonPressMask | ButtonReleaseMask |
                                PointerMotionMask,
                            GrabModeAsync,
                            GrabModeAsync, None, invisible, CurrentTime);
    }
    if (kbgrab != GrabSuccess)
    {
      kbgrab = XGrabKeyboard(dpy, lock->root, True,
                             GrabModeAsync, GrabModeAsync, CurrentTime);
    }

    /* input is grabbed: we can lock the screen */
    if (ptgrab == GrabSuccess && kbgrab == GrabSuccess)
    {
      XMapRaised(dpy, lock->win);
      if (rr->active)
        XRRSelectInput(dpy, lock->win, RRScreenChangeNotifyMask);

      XSelectInput(dpy, lock->root, SubstructureNotifyMask);
      return lock;
    }

    /* retry on AlreadyGrabbed but fail on other errors */
    if ((ptgrab != AlreadyGrabbed && ptgrab != GrabSuccess) ||
        (kbgrab != AlreadyGrabbed && kbgrab != GrabSuccess))
      break;

    usleep(100000);
  }

  /* we couldn't grab all input: fail out */
  if (ptgrab != GrabSuccess)
    fprintf(stderr, "slock: unable to grab mouse pointer for screen %d\n",
            screen);
  if (kbgrab != GrabSuccess)
    fprintf(stderr, "slock: unable to grab keyboard for screen %d\n",
            screen);
  return NULL;
}

static void
usage(void)
{
  die("usage: slock [-v] [-f] [cmd [arg ...]]\n");
}

int main(int argc, char **argv)
{
  struct xrandr rr;
  struct lock **locks;
  struct passwd *pwd;
  struct group *grp;
  uid_t duid;
  gid_t dgid;
  const char *hash;
  Display *dpy;
  int i, s, nlocks, nscreens;
  int count_fonts;
  char **font_names;
  CARD16 standby, suspend, off;

  ARGBEGIN
  {
  case 'v':
    fprintf(stderr, "slock-" VERSION "\n");
    return 0;
  case 'f':
    if (!(dpy = XOpenDisplay(NULL)))
      die("slock: cannot open display\n");
    font_names = XListFonts(dpy, "*", 10000 /* list 10000 fonts*/, &count_fonts);
    for (i = 0; i < count_fonts; i++)
    {
      fprintf(stderr, "%s\n", *(font_names + i));
    }
    return 0;
  default:
    usage();
  }
  ARGEND

  /* validate drop-user and -group */
  errno = 0;
  if (!(pwd = getpwnam(user)))
    die("slock: getpwnam %s: %s\n", user,
        errno ? strerror(errno) : "user entry not found");
  duid = pwd->pw_uid;
  errno = 0;
  if (!(grp = getgrnam(group)))
    die("slock: getgrnam %s: %s\n", group,
        errno ? strerror(errno) : "group entry not found");
  dgid = grp->gr_gid;

#ifdef __linux__
  dontkillme();
#endif

  hash = gethash();
  errno = 0;
  if (!crypt("", hash))
    die("slock: crypt: %s\n", strerror(errno));

  if (!(dpy = XOpenDisplay(NULL)))
    die("slock: cannot open display\n");

  /* drop privileges */
  if (setgroups(0, NULL) < 0)
    die("slock: setgroups: %s\n", strerror(errno));
  if (setgid(dgid) < 0)
    die("slock: setgid: %s\n", strerror(errno));
  if (setuid(duid) < 0)
    die("slock: setuid: %s\n", strerror(errno));

  /*Create screenshot Image*/
  Screen *scr = ScreenOfDisplay(dpy, DefaultScreen(dpy));
  image = imlib_create_image(scr->width, scr->height);
  imlib_context_set_image(image);
  imlib_context_set_display(dpy);
  imlib_context_set_visual(DefaultVisual(dpy, 0));
  imlib_context_set_drawable(RootWindow(dpy, XScreenNumberOfScreen(scr)));
  imlib_copy_drawable_to_image(0, 0, 0, scr->width, scr->height, 0, 0, 1);

#ifdef BLUR

  /*Blur function*/
  imlib_image_blur(blurRadius);
#endif // BLUR

#ifdef PIXELATION
  /*Pixelation*/
  int width = scr->width;
  int height = scr->height;

  for (int y = 0; y < height; y += pixelSize)
  {
    for (int x = 0; x < width; x += pixelSize)
    {
      int red = 0;
      int green = 0;
      int blue = 0;

      Imlib_Color pixel;
      Imlib_Color *pp;
      pp = &pixel;
      for (int j = 0; j < pixelSize && j < height; j++)
      {
        for (int i = 0; i < pixelSize && i < width; i++)
        {
          imlib_image_query_pixel(x + i, y + j, pp);
          red += pixel.red;
          green += pixel.green;
          blue += pixel.blue;
        }
      }
      red /= (pixelSize * pixelSize);
      green /= (pixelSize * pixelSize);
      blue /= (pixelSize * pixelSize);
      imlib_context_set_color(red, green, blue, pixel.alpha);
      imlib_image_fill_rectangle(x, y, pixelSize, pixelSize);
      red = 0;
      green = 0;
      blue = 0;
    }
  }

#endif
  /* check for Xrandr support */
  rr.active = XRRQueryExtension(dpy, &rr.evbase, &rr.errbase);

  /* get number of screens in display "dpy" and blank them */
  nscreens = ScreenCount(dpy);
  if (!(locks = calloc(nscreens, sizeof(struct lock *))))
    die("slock: out of memory\n");
  for (nlocks = 0, s = 0; s < nscreens; s++)
  {
    if ((locks[s] = lockscreen(dpy, &rr, s)) != NULL)
    {
      nlocks++;
    }
    else
    {
      break;
    }
  }
  XSync(dpy, 0);

  /* did we manage to lock everything? */
  if (nlocks != nscreens)
    return 1;

  /* DPMS magic to disable the monitor */
  if (!DPMSCapable(dpy))
    die("slock: DPMSCapable failed\n");
  if (!DPMSEnable(dpy))
    die("slock: DPMSEnable failed\n");
  if (!DPMSGetTimeouts(dpy, &standby, &suspend, &off))
    die("slock: DPMSGetTimeouts failed\n");
  if (!standby || !suspend || !off)
    die("slock: at least one DPMS variable is zero\n");
  if (!DPMSSetTimeouts(dpy, monitortime, monitortime, monitortime))
    die("slock: DPMSSetTimeouts failed\n");

  XSync(dpy, 0);

  /* run post-lock command */
  if (argc > 0)
  {
    switch (fork())
    {
    case -1:
      die("slock: fork failed: %s\n", strerror(errno));
    case 0:
      if (close(ConnectionNumber(dpy)) < 0)
        die("slock: close: %s\n", strerror(errno));
      execvp(argv[0], argv);
      fprintf(stderr, "slock: execvp %s: %s\n", argv[0], strerror(errno));
      _exit(1);
    }
  }

  /* everything is now blank. Wait for the correct password */
  readpw(dpy, &rr, locks, nscreens, hash);

  imlib_free_image();
  /* reset DPMS values to inital ones */
  DPMSSetTimeouts(dpy, standby, suspend, off);
  XSync(dpy, 0);

  return 0;
}
