/* user and group to drop privileges to */
static const char *user  = "nobody";
static const char *group = "nogroup";

static const char *colorname[NUMCOLS] = {
	[INIT] =   "black",     /* after initialization */
	[INPUT] =  "#005577",   /* during input */
	[FAILED] = "#CC3333",   /* wrong password */
	[BLOCKS] = "#ffffff",
	[BG] = 		 "#000000",
};

/* treat a cleared input like a wrong password (color) */
static const int failonclear = 1;

/* time in seconds before the monitor shuts down */
static const int monitortime = 5;

// The radius of the circle
static const unsigned int circle_radius = 500;

// Number of blocks/divisions of the bar for key feedback
static const unsigned int blocks_count = 10;

// Bar position (BAR_TOP or BAR_BOTTOM)
static const unsigned int bar_position = BAR_TOP;

// Image to show on center
static const char * icon_path = "lock.png";


/*Enable blur*/
#define BLUR
/*Set blur radius*/
static const int blurRadius=5;
/*Enable Pixelation*/
//#define PIXELATION
/*Set pixelation radius*/
static const int pixelSize=0;
