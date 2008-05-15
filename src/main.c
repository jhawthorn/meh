
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#include <X11/Xlib.h>
#include <X11/keysym.h>

#include <sys/time.h>

#include "jpeg.h"
#include "util.h"

Display *display;
int screen;
Window window;
GC gc;

void usage(){
	printf("USAGE: meh [FILE1 [FILE2 [...]]]");
	exit(1);
}

static int popcount(unsigned long mask){
#if __GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 4)
    return __builtin_popcount(mask);
#else
    int y;

    y = (mask >> 1) &033333333333;
    y = mask - y - ((y >>1) & 033333333333);
    return (((y + (y >> 3)) & 030707070707) % 077);
#endif
}


unsigned int getshift(unsigned int mask){
	return popcount((mask - 1) & ~mask) & 31;
}

XImage *create_image_from_buffer(unsigned char *buf, int width, int height, int bufwidth, int bufheight) {
	int depth;
	XImage *img = NULL;
	Visual *vis;
	unsigned int rshift, gshift, bshift;
	int outIndex = 0;	
	int i;
	int x,y;
	int numBufBytes = (3 * bufwidth * bufheight);
	
	depth = DefaultDepth(display, screen);
	vis = DefaultVisual(display, screen);

	rshift = getshift(vis->red_mask);
	gshift = getshift(vis->green_mask);
	bshift = getshift(vis->blue_mask);

	if (depth >= 24) {
		size_t numNewBufBytes = (4 * (width * height));
		u_int32_t *newBuf = malloc(numNewBufBytes);
	
		//for(outIndex = 0; outIndex < width * height; outIndex++){
		for(y = 0; y < height; y++){
			for(x = 0; x < width; x++){
				unsigned int r, g, b;
				i = (y * bufheight / height * bufwidth + x * bufwidth / width) * 3;
				assert(i < numBufBytes);
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
		
	} else if (depth >= 15) {
		exit(1);
		size_t numNewBufBytes = (2 * (width * height));
		u_int16_t *newBuf = malloc (numNewBufBytes);
		
		for (i = 0; i < numBufBytes;) {
			unsigned int r, g, b;
			r = (buf[i++] << rshift) & vis->red_mask;
			g = (buf[i++] << gshift) & vis->green_mask;
			b = (buf[i++] << bshift) & vis->blue_mask;
			
			newBuf[outIndex++] = r | g | b;
		}		
		
		img = XCreateImage (display,
			CopyFromParent, depth,
			ZPixmap, 0,
			(char *) newBuf,
			width, height,
			16, 0
		);
	} else {
		fprintf (stderr, "This program does not support displays with a depth less than 15.");
		exit(1);
		return NULL;				
	}

	XInitImage (img);

	return img;
}		

void init(){
	display = XOpenDisplay (NULL);
	assert(display);
	screen = DefaultScreen(display);

	window = XCreateWindow(display, DefaultRootWindow(display), 0, 0, 640, 480, 0, DefaultDepth(display, screen), InputOutput, CopyFromParent, 0, NULL);
	gc = XCreateGC(display, window, 0, NULL);

	XMapRaised(display, window);
	XSelectInput(display, window, StructureNotifyMask | ExposureMask | KeyPressMask);
	XFlush(display);
}

void run(char *images[], int length){
	int i = 0;
	int bufwidth = 0, bufheight = 0;
	int width = 0, height = 0;
	XImage *img = NULL;
	unsigned char *buf = NULL;
	int redraw = 0;

	for(;;){
		XEvent event;
		while(XPending(display)){
			XNextEvent(display, &event);
			switch(event.type){
				case MapNotify:
					break;
				case ConfigureNotify:
					if(width != event.xconfigure.width || height != event.xconfigure.height){
						if(img)
							free(img);
						img = NULL;
						width = event.xconfigure.width;
						height = event.xconfigure.height;
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
							i = (i + 1) % length;
							if(img)
								free(img);
							if(buf)
								free(buf);
							img = NULL;
							buf = NULL;
							redraw = 1;
							break;
						case XK_n:
							i = i ? i - 1 : length - 1;
							if(img)
								free(img);
							if(buf)
								free(buf);
							img = NULL;
							buf = NULL;
							redraw = 1;
							break;
					}
					break;
			}
		}
		if(redraw){
			struct timeval tv0;
			struct timeval tv1;
			if(!buf){
				printf("loading...");
				gettimeofday(&tv0, NULL);
				buf = loadjpeg(images[i], &bufwidth, &bufheight);
				assert(buf);
				gettimeofday(&tv1, NULL);
				printf(" %i milliseconds\n", (tv1.tv_sec - tv0.tv_sec) * 1000 + (tv1.tv_usec - tv0.tv_usec)/1000);
			}
			if(!img){
				printf("scaling & converting...");
				gettimeofday(&tv0, NULL);
				img = create_image_from_buffer(buf, width, height, bufwidth, bufheight);
				assert(img);
				gettimeofday(&tv1, NULL);
				printf(" %i milliseconds\n", (tv1.tv_sec - tv0.tv_sec) * 1000 + (tv1.tv_usec - tv0.tv_usec)/1000);
			}
			printf("Displaying\n");
			XPutImage(display, window, gc, img, 0, 0, 0, 0, width, height);
			XFlush(display);
			redraw = 0;
		}
	}
}

int main(int argc, char *argv[]){
	if(argc < 2)
		usage();
	init();

	run(&argv[1], argc - 1);

	return 0;
}

