/* user and group to drop privileges to */
static const char *user  = "sepseel";
static const char *group = "sepseel";

static const char *colorname[NUMCOLS] = {
	[INIT] =   "black",     /* after initialization */
	[INPUT] =  "#0000ff",   /* during input */
	[FAILED] = "#ff0000",   /* wrong password */
	[BLOCKS] = "#ffffff",
	[BG] = 		 "#0000ff",
};

/* treat a cleared input like a wrong password (color) */
static const int failonclear = 1;

/* time in seconds before the monitor shuts down */
static const int monitortime = 60;

// The radius of the circle
static const unsigned int circle_radius = 300;

// Number of blocks/divisions of the bar for key feedback
static const unsigned int blocks_count = 10;

// Bar position (BAR_TOP or BAR_BOTTOM)
static const unsigned int bar_position = BAR_TOP;

// Icon to draw in the center
static const char * icon_path = "/home/sepseel/images/icon.png";

/*Enable blur*/
#define BLUR
/*Set blur radius*/
static const int blurRadius=10;
/*Enable Pixelation*/
//#define PIXELATION
/*Set pixelation radius*/
static const int pixelSize=0;
