/* user and group to drop privileges to */
static const char *user  = "sepseel";
static const char *group = "sepseel";

static const char *colorname[NUMCOLS] = {
	[INIT] =   "#a1efe4",     /* after initialization */
	[INPUT] =  "#a6e22e",   /* during input */
	[FAILED] = "#f92672",   /* wrong password */
};

/* treat a cleared input like a wrong password (color) */
static const int failonclear = 1;

/* time in seconds before the monitor shuts down */
static const int monitortime = 60;

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
