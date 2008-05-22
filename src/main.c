
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <X11/Xlib.h>
#include <X11/keysym.h>

#include <sys/time.h>

#include "meh.h"

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

unsigned char *loadbuf(const char *filename, int *bufwidth, int *bufheight){
	FILE *f;
	unsigned char head[4];
	unsigned char *buf;

	if((f = fopen(filename, "rb")) == NULL){
		fprintf(stderr, "Cannot open '%s'\n", filename);
		return NULL;
	}
	memset(head, 0, 4);
	fread(head, 1, 4, f);
	rewind(f);
	if(head[0] == 0xff && head[1] == 0xd8){
		buf = loadjpeg(f, bufwidth, bufheight);
	}else if(!memcmp("\x89PNG", head, 4)){
		buf = loadpng(f, bufwidth, bufheight);
	}else if(!memcmp("GIF", head, 3)){
		buf = loadgif(f, bufwidth, bufheight);
	}else{
		fprintf(stderr, "Unknown file type: '%s'\n", filename);
		buf = NULL;
	}
	fclose(f);
	return buf;
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
	int xoffset = 0, yoffset = 0;
	int imagewidth = 0, imageheight = 0;
	int width = 0, height = 0;
	int fillw = 0, fillh = 0;
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
			if(!buf){
				buf = loadbuf(images[i], &bufwidth, &bufheight);
				assert(buf);
			}
			if(!img){
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
				}
				img = create_image_from_buffer(buf, imagewidth, imageheight, bufwidth, bufheight);
				assert(img);
			}
			printf("Displaying\n");
			XFillRectangle(display, window, gc, 0, 0, fillw, fillh);
			XPutImage(display, window, gc, img, 0, 0, xoffset, yoffset, width, height);
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

	run(&argv[1], argc - 1);

	return 0;
}

