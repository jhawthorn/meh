#ifndef MEH_H
#define MEH_H MEH_H

#include <stdio.h>
#include <sys/select.h> /* for fd_set */

struct image;

struct imageformat{
	struct image *(*open)(FILE *);
	void (*prep)(struct image *);
	int (*read)(struct image *);
	void (*close)(struct image *);
};

typedef enum{
	NONE = 0,
	LOADED = 1,
	SLOWLOADED = 2,
	SCALED = 4,
	SLOWSCALED = 8,
	DRAWN = 16
} imgstate;

struct image{
	unsigned char *buf;
	unsigned int bufwidth, bufheight;
	struct imageformat *fmt;
	imgstate state;
	int idx;
	void *backend;
};


/* backend */
void backend_init();
void backend_free(struct image *img);
void backend_setaspect(unsigned int w, unsigned int h);
void backend_prepare(struct image *img, unsigned int width, unsigned int height, int fast);
void backend_draw(struct image *img, unsigned int width, unsigned int height);
void backend_run();

/* key actions for backend */
void key_reload();
void key_next();
void key_prev();
void key_quit();
void key_action();
void key_default(char *key);

/* callbacks from backend */
int setup_fds(fd_set *fds);
int process_fds(fd_set *fds);
int process_idle();

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

extern int width, height;
extern struct image *curimg;

#endif

