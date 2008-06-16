
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
extern struct imageformat jpeg;
struct imageformat *formats[] = {
	&jpeg,
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

static int popcount(unsigned long mask){
#if __GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 4)
    return __builtin_popcount(mask);
#else
    int y;

    y = (mask >> 1) & 033333333333;
    y = mask - y - ((y >> 1) & 033333333333);
    return (((y + (y >> 3)) & 030707070707) % 077);
#endif
}


unsigned int getshift(unsigned int mask){
	return popcount((mask - 1) & ~mask) & 31;
}

void setaspect(int w, int h){
	XSizeHints *hints = XAllocSizeHints();
	hints->flags = PAspect;
	hints->min_aspect.x = hints->max_aspect.x = w;
	hints->min_aspect.y = hints->max_aspect.y = h;
	XSetWMNormalHints(display, window, hints);
	XFree(hints);
}

XImage *create_image_from_buffer(unsigned char *buf, int width, int height, int bufwidth, int bufheight) {
	int depth;
	XImage *img = NULL;
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
		size_t numNewBufBytes = (4 * (width * height));
		u_int32_t *newBuf = malloc(numNewBufBytes);
	
		for(y = 0; y < height; y++){
			for(x = 0; x < width; x++){
				unsigned int r, g, b;
				i = (y * bufheight / height * bufwidth + x * bufwidth / width) * 3;
				r = (buf[i++] << rshift) & vis->red_mask;
				g = (buf[i++] << gshift) & vis->green_mask;
				b = (buf[i++] << bshift) & vis->blue_mask;
				
				newBuf[y * width + x] = r | g | b;
			}
		}
		
		img = XCreateImage (display, 
			CopyFromParent, depth, 
			ZPixmap, 0, 
			(char *) newBuf,
			width, height,
			32, 0
		);
	}else if(depth >= 15){
		size_t numNewBufBytes = (2 * (width * height));
		u_int16_t *newBuf = malloc (numNewBufBytes);
		
		for(y = 0; y < height; y++){
			for(x = 0; x < width; x++){
				unsigned int r, g, b;
				i = (y * bufheight / height * bufwidth + x * bufwidth / width) * 3;
				r = (buf[i++] << rshift) & vis->red_mask;
				g = (buf[i++] << gshift) & vis->green_mask;
				b = (buf[i++] << bshift) & vis->blue_mask;
				
				newBuf[y * width + x] = r | g | b;
			}
		}
		
		img = XCreateImage(display,
			CopyFromParent, depth,
			ZPixmap, 0,
			(char *) newBuf,
			width, height,
			16, 0
		);
	}else{
		fprintf (stderr, "This program does not support displays with a depth less than 15.");
		exit(1);
		return NULL;				
	}

	XInitImage (img);

	return img;
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

unsigned char *loadbuf(const char *filename, int *bufwidth, int *bufheight){
	struct image *img;
	img = imgopen(filename);
	img->buf = malloc(3 * img->width * img->height);
	*bufwidth = img->width;
	*bufheight = img->height;
	img->fmt->read(img);
	return img->buf;
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
	int bufwidth = 0, bufheight = 0;
	int xoffset = 0, yoffset = 0;
	int imagewidth = 0, imageheight = 0;
	int width = 0, height = 0;
	int fillw = 0, fillh = 0;
	XImage *ximg = NULL;
	struct image *img = NULL;
	unsigned char *buf = NULL;
	int redraw = 0;

	for(;;){
		XEvent event;
		for(;;){
			if(redraw && !XPending(display)){
				break;
			}
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
							if(buf)
								free(buf);
							ximg = NULL;
							buf = NULL;
							redraw = 1;
							break;
						case XK_Return:
							printf("%s\n", image->filename);
							break;
					}
					break;
			}
		}
		if(redraw){
			while(!buf){
				buf = loadbuf(image->filename, &bufwidth, &bufheight);
				if(!buf){
					if(image->next == image){
						fprintf(stderr, "No valid images to view\n");
						exit(1);
					}
					struct imagenode *tmp = image;
					if(direction < 0){
						image = image->prev;
					}else{
						image = image->next;
					}
					tmp->prev->next = tmp->next;
					tmp->next->prev = tmp->prev;
					free(tmp);
				}
				setaspect(bufwidth, bufheight);
				continue;
			}
			if(!ximg){
				if(width * bufheight > height * bufwidth){
					imagewidth = bufwidth * height / bufheight;
					imageheight = height;
					xoffset = (width - imagewidth) / 2;
					yoffset = 0;
					fillw = xoffset;
					fillh = height;
				}else if(width * bufheight < height * bufwidth){
					imagewidth = width;
					imageheight = bufheight * width / bufwidth;
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
				ximg = create_image_from_buffer(buf, imagewidth, imageheight, bufwidth, bufheight);
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
	if(argc < 2)
		usage();
	init();
	struct imagenode *list = buildlist(argc - 1, &argv[1]);

	run(list);

	return 0;
}

