
#include <stdio.h>

#include "X11/Xlib.h"

struct image;

struct imageformat{
	struct image *(*open)(FILE *);
	void (*prep)(struct image *);
	int (*read)(struct image *);
	void (*close)(struct image *);
};

typedef enum{
	NONE,
	FASTLOADED,
	FASTSCALED,
	FASTDRAWN,
	LOADED,
	SCALED,
	DRAWN,
	NOOP
} imgstate;

struct image{
	unsigned char *buf;
	unsigned int bufwidth, bufheight;
	struct imageformat *fmt;
	imgstate state;
	XImage *ximg;
};

void setaspect(unsigned int w, unsigned int h);
void xinit();
void drawimage(XImage *ximg, unsigned int width, unsigned int height);
XImage *getimage(struct image *img, unsigned int width, unsigned int height, int fast);
void freeXImg(XImage *ximg);

#ifdef TDEBUG
#define TDEBUG_START \
struct timeval t0;\
struct timeval t1;\
gettimeofday(&t0, NULL);
#define TDEBUG_END(x) \
gettimeofday(&t1, NULL); \
printf("%s: %li e2 us\n", (x), ((t1.tv_sec - t0.tv_sec) * 1000000 + t1.tv_usec - t0.tv_usec) / 100);
#else
#define TDEBUG_START
#define TDEBUG_END(x)
#endif

/* Supported Formats */
extern struct imageformat libjpeg;
extern struct imageformat giflib;
extern struct imageformat libpng;
extern struct imageformat bmp;
extern struct imageformat netpbm;
extern struct imageformat imagemagick;



