
#include <stdio.h>

struct image;

struct imageformat{
	struct image *(*open)(FILE *);
	int (*read)(struct image *);
};

struct image{
	unsigned char *buf;
	int width, height;
	FILE *f;
	struct imageformat *fmt;
};

#include "X11/Xlib.h"

XImage *ximage(struct image *img, int width, int height);
void setaspect(int w, int h);
void xinit();
void drawimage(struct image *img, int width, int height);

