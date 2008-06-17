
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include <sys/time.h>

#include "meh.h"

/* Supported Formats */
extern struct imageformat libjpeg;
extern struct imageformat giflib;
extern struct imageformat libpng;
struct imageformat *formats[] = {
	&libjpeg,
	&giflib,
	&libpng,
	NULL
};

/* Globals */
Display *display;
int screen;
Window window;
GC gc;

void usage(){
	printf("USAGE: meh [FILE1 [FILE2 [...]]]\n");
	exit(1);
}

unsigned int getshift(unsigned int mask){
#if __GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 4)
	return __builtin_ctz(mask);
#else
	int i = 0;
	while((mask & 1) == 0){
		i++;
		mask >>= 1;
	}
	return i;
#endif
}

void setaspect(int w, int h){
	XSizeHints *hints = XAllocSizeHints();
	hints->flags = PAspect;
	hints->min_aspect.x = hints->max_aspect.x = w;
	hints->min_aspect.y = hints->max_aspect.y = h;
	XSetWMNormalHints(display, window, hints);
	XFlush(display);
	XFree(hints);
}

XImage *ximage(struct image *img, int width, int height) {
	int depth;
	XImage *ximg = NULL;
	Visual *vis;
	unsigned int rshift, gshift, bshift;
	int i;
	int x,y;

	depth = DefaultDepth(display, screen);
	vis = DefaultVisual(display, screen);

	rshift = getshift(vis->red_mask);
	gshift = getshift(vis->green_mask);
	bshift = getshift(vis->blue_mask);

	if (depth >= 24) {
		unsigned int dx;
		size_t numNewBufBytes = (4 * width * height);
		u_int32_t *newBuf = malloc(numNewBufBytes);
	
		dx = 1024 * img->width / width;
		for(y = 0; y < height; y++){
			i = (y * img->height / height * img->width) * 1024;
			for(x = 0; x < width; x++){
				unsigned int r, g, b;
				r = (img->buf[(i >> 10)*3] << rshift) & vis->red_mask;
				g = (img->buf[(i >> 10)*3+1] << gshift) & vis->green_mask;
				b = (img->buf[(i >> 10)*3+2] << bshift) & vis->blue_mask;
				
				newBuf[y * width + x] = r | g | b;
				i += dx;
			}
		}

		ximg = XCreateImage (display, 
			CopyFromParent, depth, 
			ZPixmap, 0, 
			(char *) newBuf,
			width, height,
			32, 0
		);
	}else if(depth >= 15){
		unsigned int dx;
		size_t numNewBufBytes = (2 * width * height);
		u_int16_t *newBuf = malloc (numNewBufBytes);

		dx = 1024 * img->width / width;
		for(y = 0; y < height; y++){
			i = (y * img->height / height * img->width) * 1024;
			for(x = 0; x < width; x++){
				unsigned int r, g, b;
				r = (img->buf[i++] << rshift) & vis->red_mask;
				g = (img->buf[i++] << gshift) & vis->green_mask;
				b = (img->buf[i++] << bshift) & vis->blue_mask;
				
				newBuf[y * width + x] = r | g | b;
				i += dx;
			}
		}
		
		ximg = XCreateImage(display,
			CopyFromParent, depth,
			ZPixmap, 0,
			(char *) newBuf,
			width, height,
			16, 0
		);
	}else{
		fprintf(stderr, "This program does not support displays with a depth less than 15.\n");
		exit(1);
		return NULL;				
	}

	XInitImage(ximg);

	return ximg;
}

struct image *imgopen(const char *filename){
	struct image *img = NULL;
	struct imageformat **fmt = formats;
	FILE *f;
	if((f = fopen(filename, "rb")) == NULL){
		fprintf(stderr, "Cannot open '%s'\n", filename);
		return NULL;
	}
	for(fmt = formats; *fmt; fmt++){
		if((img = (*fmt)->open(f))){
			img->fmt = *fmt;
			return img;
		}
	}
	fprintf(stderr, "Unknown file type: '%s'\n", filename);
	return NULL;
}

struct imagenode{
	struct imagenode *next, *prev;
	char *filename;
};

struct imagenode *buildlist(int argc, char *argv[]){
	struct imagenode *head = NULL, *tail = NULL, *tmp;
	if(argc){
		while(argc--){
			tmp = malloc(sizeof(struct imagenode));
			if(!head)
				head = tail = tmp;
			tmp->filename = *argv++;
			tmp->prev = tail;
			tail->next = tmp;
			tail=tmp;
		}
		tail->next = head;
		head->prev = tail;
		return head;
	}else{
		fprintf(stderr, "No files to view\n");
		exit(1);
	}
}


void init(){
	display = XOpenDisplay (NULL);
	assert(display);
	screen = DefaultScreen(display);

	window = XCreateWindow(display, DefaultRootWindow(display), 0, 0, 640, 480, 0, DefaultDepth(display, screen), InputOutput, CopyFromParent, 0, NULL);
	setaspect(1, 1);
	gc = XCreateGC(display, window, 0, NULL);

	XMapRaised(display, window);
	XSelectInput(display, window, StructureNotifyMask | ExposureMask | KeyPressMask);
	XFlush(display);
}

void run(struct imagenode *image){
	int direction = 1;
	int xoffset = 0, yoffset = 0;
	int imagewidth = 0, imageheight = 0;
	int width = 0, height = 0;
	int fillw = 0, fillh = 0;
	XImage *ximg = NULL;
	struct image *img = NULL;
	int redraw = 0;

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
						if(ximg){
							free(ximg->data);
							XFree(ximg);
						}
						ximg = NULL;
						width = event.xconfigure.width;
						height = event.xconfigure.height;
						redraw = 1;
						if(img)
							setaspect(img->width, img->height); /* Some window managers need reminding */
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
								image = image->next;
								direction = 1;
							}else{
								image = image->prev;
								direction = -1;
							}
							if(ximg){
								free(ximg->data);
								XFree(ximg);
							}
							if(img){
								if(img->buf)
									free(img->buf);
								free(img);
							}
							ximg = NULL;
							img = NULL;
							redraw = 1;
							break;
						case XK_Return:
							break;
					}
					break;
			}
		}
		if(redraw){
			if(!img){
				while(!(img = imgopen(image->filename))){
					struct imagenode *tmp = image;
					if(image->next == image){
						fprintf(stderr, "No valid images to view\n");
						exit(1);
					}
					if(direction < 0){
						image = image->prev;
					}else{
						image = image->next;
					}
					tmp->prev->next = tmp->next;
					tmp->next->prev = tmp->prev;
					free(tmp);
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
				continue; /* Allow for some events to be read, read is slow */
			}
			if(!ximg){
				if(width * img->height > height * img->width){
					imagewidth = img->width * height / img->height;
					imageheight = height;
					xoffset = (width - imagewidth) / 2;
					yoffset = 0;
					fillw = xoffset;
					fillh = height;
				}else if(width * img->height < height * img->width){
					imagewidth = width;
					imageheight = img->height * width / img->width;
					xoffset = 0;
					yoffset = (height - imageheight) / 2;
					fillw = width;
					fillh = yoffset;
				}else{
					xoffset = 0;
					yoffset = 0;
					fillw = 0;
					fillh = 0;
					imagewidth = width;
					imageheight = height;
				}
				ximg = ximage(img, imagewidth, imageheight);
				assert(ximg);
			}
			XFillRectangle(display, window, gc, 0, 0, fillw, fillh);
			XPutImage(display, window, gc, ximg, 0, 0, xoffset, yoffset, width, height);
			XFillRectangle(display, window, gc, width - fillw, height - fillh, fillw, fillh);
			XFlush(display);
			redraw = 0;
		}
	}
}

int main(int argc, char *argv[]){
	struct imagenode *list;

	if(argc < 2)
		usage();
	init();

	list = buildlist(argc - 1, &argv[1]);
	run(list);

	return 0;
}

