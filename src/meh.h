
#include <stdio.h>

#include "X11/Xlib.h"

struct image;

struct imageformat{
	struct image *(*open)(FILE *);
	int (*read)(struct image *);
	void (*close)(struct image *);
};

typedef enum{
	NONE,
	IMG,
	ALLOC,
	LINEAR,
	LINEARDRAWN,
	BILINEAR,
	BILINEARDRAWN,
} drawstate;

struct image{
	unsigned char *buf;
	unsigned int bufwidth, bufheight;
	struct imageformat *fmt;
	int redraw;
	drawstate state;
	XImage *ximg;
};

XImage *ximage(struct image *img, unsigned int width, unsigned int height);
void setaspect(unsigned int w, unsigned int h);
void xinit();
void drawimage(XImage *ximg, unsigned int width, unsigned int height);
XImage *getimage(struct image *img, unsigned int width, unsigned int height);

