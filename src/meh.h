
#include <stdio.h>

struct image;

struct imageformat{
	struct image *(*open)(FILE *);
	int (*read)(struct image *);
};

struct image{
	unsigned char *buf;
	unsigned int width, height;
	FILE *f;
	struct imageformat *fmt;
};

#include "X11/Xlib.h"

XImage *ximage(struct image *img, unsigned int width, unsigned int height);
void setaspect(unsigned int w, unsigned int h);
void xinit();
void drawimage(XImage *ximg, unsigned int width, unsigned int height);
XImage *getimage(struct image *img, int width, int height);

