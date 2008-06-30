
#include <unistd.h>

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include "meh.h"

#define MODE_NORM 0
#define MODE_LIST 1
#define MODE_CTL 2
static int mode;

extern Display *display;

/* Supported Formats */
extern struct imageformat libjpeg;
extern struct imageformat giflib;
extern struct imageformat libpng;
extern struct imageformat bmp;
struct imageformat *formats[] = {
	&libjpeg,
	&bmp,
	&libpng,
	&giflib, /* HACK! make gif last (uses read()) */
	NULL
};


void usage(){
	printf("USAGE: meh [FILE1 [FILE2 [...]]]\n");
	printf("       meh -list                 : treat stdin as list of files\n");
	printf("       meh -ctl                  : display files as they are received on stdin\n");
	exit(EXIT_FAILURE);
}

struct image *imgopen(FILE *f){
	struct image *img = NULL;
	struct imageformat **fmt = formats;
	for(fmt = formats; *fmt; fmt++){
		if((img = (*fmt)->open(f))){
			img->fmt = *fmt;
			return img;
		}
	}
	return NULL;
}

static int imageslen;
static int imageidx;
static char **images;

static char *filename = NULL;
static int width = 0, height = 0;
static struct image *img = NULL;
static XImage *ximg = NULL;
static int redraw = 0;

void nextimage(){
	if(++imageidx == imageslen)
		imageidx = 0;
	filename = images[imageidx];
}

void previmage(){
	if(--imageidx < 0)
		imageidx = imageslen - 1;
	filename = images[imageidx];
}

static void (*direction)() = nextimage;

void handlekeypress(XEvent *event){
	KeySym key = XLookupKeysym(&event->xkey, 0);
	switch(key){
		case XK_Escape:
		case XK_q:
			exit(0);
			break;
		case XK_Return:
			puts(filename);
			fflush(stdout);
			break;
		case XK_t:
		case XK_n:
			if(mode == MODE_CTL)
				return;
			direction = key == XK_t ? nextimage : previmage;
			direction();
			/* Pass through */
		case XK_r:
			if(img){
				if(img->buf)
					free(img->buf);
				free(img);
			}
			if(ximg)
				XDestroyImage(ximg);
			ximg = NULL;
			img = NULL;
			redraw = 1;
			break;
	}
}

void handleevent(XEvent *event){
	switch(event->type){
		case ConfigureNotify:
			if(width != event->xconfigure.width || height != event->xconfigure.height){
				width = event->xconfigure.width;
				height = event->xconfigure.height;
				redraw = 1;
				if(ximg)
					XDestroyImage(ximg);
				ximg = NULL;

				/* Some window managers need reminding */
				if(img)
					setaspect(img->width, img->height);
			}
			break;
		case Expose:
			redraw = 1;
			break;
		case KeyPress:
			handlekeypress(event);
			break;
	}
}

void doredraw(){
	if(!img){
		if(!filename)
			return;
		const char *firstimg = filename;
		while(!img){
			FILE *f;
			if((f = fopen(filename, "rb"))){
				if((img = imgopen(f)))
					break;
				else
					fprintf(stderr, "Invalid format '%s'\n", filename);
			}else{
				fprintf(stderr, "Cannot open '%s'\n", filename);
			}
			if(mode == MODE_CTL)
				return;
			direction();
			if(filename == firstimg){
				fprintf(stderr, "No valid images to view\n");
				exit(EXIT_FAILURE);
			}
		}

		setaspect(img->width, img->height);

		img->buf = malloc(3 * img->width * img->height);
		if(img->fmt->read(img)){
			fprintf(stderr, "read error!\n");
		}
		img->fmt->close(img);

		/* Allow for some events to be read, read is slow */
		/*while(XPending(display)){
			XEvent event;
			XNextEvent(display, &event);
			handleevent(&event);
		}*/
	}
	if(!ximg)
		ximg = getimage(img, width, height);
	drawimage(ximg, width, height);
}


void run(){
	int xfd;
	xfd = ConnectionNumber(display);
	fd_set fds;
	struct timeval tv0 = {0, 0};
	struct timeval *tv = &tv0;

	char buf[512];
	int bufidx = 0;
	for(;;){
		FD_ZERO(&fds);
		FD_SET(xfd, &fds);
		if(mode == MODE_CTL)
			FD_SET(0, &fds); /* STDIN */
		int ret = select(xfd+1, &fds, NULL, NULL, tv);
		if(ret == -1){
			perror("select failed\n");
		}
		if(FD_ISSET(0, &fds)){
			assert(mode == MODE_CTL);
			int n = read(0, buf, 512 - bufidx);
			if(n == -1){
				perror("read failed");
			}else if(n == 0){
				fprintf(stderr, "done reading\n");
				exit(0);
			}
			char *p;
			p = &buf[bufidx];
			bufidx+=n;
			for(; n > 0; p++, n--){
				if(*p == '\n' || *p == '\0'){
					n--;
					*p = '\0';
					strcpy(filename, buf);
					for(;n && (*p == '\0' || *p == '\n'); p++, n--);
					bufidx = n;
					if(n)
						memmove(buf, p, n);
					if(img){
						if(img->buf)
							free(img->buf);
						free(img);
					}
					if(ximg)
						XDestroyImage(ximg);
					ximg = NULL;
					img = NULL;
					redraw = 1;
					tv = &tv0;
				}
			}
		}
		if(!XPending(display) && ret == 0 && redraw){
			doredraw();
			tv = NULL;
		}
		while(XPending(display)){
			tv = &tv0;
			XEvent event;
			XNextEvent(display, &event);
			handleevent(&event);
		}
	}
}

int main(int argc, char *argv[]){
	if(argc < 2)
		usage();

	if(!strcmp(argv[1], "-ctl")){
		if(argc != 2)
			usage();
		mode = MODE_CTL;
		filename = malloc(512);
		filename[0] = '\0';
	}else if(!strcmp(argv[1], "-list")){
		if(argc != 2)
			usage();
		mode = MODE_LIST;
		printf("not implemented\n");
		exit(EXIT_FAILURE);
	}else{
		mode = MODE_NORM;
		images = &argv[1];
		imageslen = argc-1;
		imageidx = 0;
		filename = argv[1];
	}
	xinit();
	run();

	return 0;
}

