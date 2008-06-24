
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include "meh.h"

/* Globals */
Display *display;
int screen;
Window window;
GC gc;

static unsigned int getshift(unsigned int mask){
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

XImage *ximage(struct image *img, unsigned int width, unsigned int height) {
	int depth;
	XImage *ximg = NULL;
	Visual *vis;
	unsigned int rshift, gshift, bshift;
	unsigned int i;
	unsigned int x,y;
	unsigned int j = 0;

	depth = DefaultDepth(display, screen);
	vis = DefaultVisual(display, screen);

	rshift = getshift(vis->red_mask);
	gshift = getshift(vis->green_mask);
	bshift = getshift(vis->blue_mask);

	struct timeval tv0, tv1;

	if (depth >= 24) {
		unsigned char *ibuf;
		unsigned int dx;
		size_t numNewBufBytes = ((sizeof(u_int32_t)) * (width) * (height));
		u_int32_t *newBuf = malloc(numNewBufBytes);
	
		dx = 1024 * img->width / width;
		for(y = 0; y < height; y++){
			i = 0;
			ibuf = &img->buf[y * img->height / height * img->width * 3];
			for(x = 0; x < width; x++){
				unsigned int r, g, b;
				r = (ibuf[(i >> 10)*3] << rshift) & vis->red_mask;
				g = (ibuf[(i >> 10)*3+1] << gshift) & vis->green_mask;
				b = (ibuf[(i >> 10)*3+2] << bshift) & vis->blue_mask;
				
				newBuf[j++] = r | g | b;
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
	}else{
		/* TODO other depths */
		fprintf(stderr, "This program does not support display depths less than 24.\n");
		exit(1);
		return NULL;				
	}

	XInitImage(ximg);

	return ximg;
}


void drawimage(struct image *img, unsigned int width, unsigned int height){
	static struct image *lastimg = NULL;
	static int lastwidth = 0, lastheight = 0;
	static XImage *ximg = NULL;
	if(0 && img == lastimg && width == lastwidth && height == lastheight){
	}else{
		if(ximg)
			XDestroyImage(ximg);
		lastwidth = width;
		lastheight = height;
		lastimg = img;
		if(width * img->height > height * img->width){
			ximg = ximage(img, img->width * height / img->height, height);
		}else{
			ximg = ximage(img, width, img->height * width / img->width);
		}
	}
	assert(ximg);
	{
		XRectangle rects[2];
		int yoffset, xoffset;
		xoffset = (width - ximg->width) / 2;
		yoffset = (height - ximg->height) / 2;
		if(xoffset || yoffset){
			rects[0].x = rects[0].y = 0;
			if(xoffset){
				rects[0].width = rects[1].width = xoffset;
				rects[0].height = rects[1].height = height;
				rects[1].x = width-xoffset;
				rects[1].y = 0;
			}else if(yoffset){
				rects[0].width = rects[1].width = width;
				rects[0].height = rects[1].height = yoffset;
				rects[1].x = 0;
				rects[1].y = height - yoffset;
			}
			XFillRectangles(display, window, gc, rects, 2);
		}
		XPutImage(display, window, gc, ximg, 0, 0, xoffset, yoffset, ximg->width, ximg->height);
		XFlush(display);
	}
}


void setaspect(unsigned int w, unsigned int h){
	XSizeHints *hints = XAllocSizeHints();
	hints->flags = PAspect;
	hints->min_aspect.x = hints->max_aspect.x = w;
	hints->min_aspect.y = hints->max_aspect.y = h;
	XSetWMNormalHints(display, window, hints);
	XFlush(display);
	XFree(hints);
}

void xinit(){
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



