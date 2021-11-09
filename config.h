/* user and group to drop privileges to */
static const char *user = "sevbesau";
static const char *group = "sevbesau";

#include "/home/sevbesau/.cache/wal/colors-wal-slock.h"

/* treat a cleared input like a wrong password (color) */
static const int failonclear = 1;

/* time in seconds before the monitor shuts down */
static const int monitortime = 60;

// Icon to draw in the center
static const char *icon_path = "/home/sevbesau/images/assets/lock.png";

/*Enable blur*/
#define BLUR
/*Set blur radius*/
static const int blurRadius = 10;
/*Enable Pixelation*/
//#define PIXELATION
/*Set pixelation radius*/
static const int pixelSize = 0;
