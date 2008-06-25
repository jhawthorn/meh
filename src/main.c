
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include "meh.h"

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
	exit(1);
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

int imageslen;
int imageidx;
char **images;

const char *nextimage(){
	if(++imageidx == imageslen)
		imageidx = 0;
	return images[imageidx];
}

const char *previmage(){
	if(--imageidx < 0)
		imageidx = imageslen - 1;
	return images[imageidx];
}

void run(){
	const char *(*direction)() = nextimage;
	const char *filename = direction();
	int width = 0, height = 0;
	struct image *img = NULL;
	XImage *ximg = NULL;
	int redraw = 0;
	FILE *f = NULL;

	for(;;){
		XEvent event;
		for(;;){
			if(redraw && !XPending(display))
				break;
			XNextEvent(display, &event);
			switch(event.type){
				case MapNotify:
					break;
				case ConfigureNotify:
					if(width != event.xconfigure.width || height != event.xconfigure.height){
						width = event.xconfigure.width;
						height = event.xconfigure.height;
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
					switch(XLookupKeysym(&event.xkey, 0)){
						case XK_Escape:
							exit(0);
							break;
						case XK_q:
							exit(0);
							break;
						case XK_t:
						case XK_n:
							if(XLookupKeysym(&event.xkey, 0) == XK_t){
								direction = nextimage;
							}else{
								direction = previmage;
							}
							filename = direction();
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
						case XK_Return:
							puts(filename);
							fflush(stdout);
							break;
					}
					break;
			}
		}
		if(redraw){
			if(!img){
				const char *firstimg = filename;
				while(!img){
					if(f){
						fclose(f);
						f = NULL;
					}
					if(!(f = fopen(filename, "rb")))
						fprintf(stderr, "Cannot open '%s'\n", filename);

					if(f){
						if((img = imgopen(f)))
							break;
						else
							fprintf(stderr, "Invalid format '%s'\n", filename);
					}

					filename = direction();
					if(filename == firstimg){
						fprintf(stderr, "No valid images to view\n");
						exit(1);
					}
				}
				img->buf = NULL;
				setaspect(img->width, img->height);
				continue; /* make sure setaspect can do its thing */
			}
			if(!img->buf){
				img->buf = malloc(3 * img->width * img->height);
				if(img->fmt->read(img)){
					fprintf(stderr, "read error!\n");
				}
				if(f){
					fclose(f);
					f = NULL;
				}
				continue; /* Allow for some events to be read, read is slow */
			}
			ximg = getimage(img, width, height);
			drawimage(ximg, width, height);
			redraw = 0;
		}
	}
}

int main(int argc, char *argv[]){
	if(argc < 2)
		usage();

	if(!strcmp(argv[1], "-ctl")){
		if(argc != 2)
			usage();
		printf("not implemented\n");
		exit(1);
	}else if(!strcmp(argv[1], "-list")){
		if(argc != 2)
			usage();
		printf("not implemented\n");
		exit(1);
	}else{
		images = &argv[1];
		imageslen = argc-1;
		imageidx = -1;
	}
	xinit();
	run();

	return 0;
}

