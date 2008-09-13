
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/extensions/XShm.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include "meh.h"

/* Globals */
Display *display;
int screen;
Window window;
GC gc;

int xshm = 0;

#define GETVAL(x, c) (ibuf[(x) * 3 + (c)])
#define GETLVAL(x, y, u, v, c) (( \
			(GETVAL((x), (c)) * (1023^(u)) + GETVAL((x)+1, (c)) * (u)) * (1023^(v)) + \
			(GETVAL((x)+img->bufwidth, (c)) * (1023^(u)) + GETVAL((x)+img->bufwidth+1, (c)) * (u)) * (v)) >> 20)

void scale(struct image *img, XImage *ximg){
	int x, y;
	unsigned char * __restrict__ ibuf;
	char* __restrict__ newBuf = ximg->data;
	unsigned int jdy = ximg->bytes_per_line / 4 - ximg->width;
	unsigned int dx = (img->bufwidth << 10) / ximg->width;

	struct timeval t0;
	struct timeval t1;
	gettimeofday(&t0, NULL);

	for(y = 0; y < ximg->height; y++){
		unsigned int bufy = (y << 10) * img->bufheight / ximg->height;
		unsigned int v_ratio = (bufy & 1023);
		unsigned int bufx = img->bufwidth / ximg->width;
		unsigned char *ibuf = &img->buf[y * img->bufheight / ximg->height * img->bufwidth * 3];
		for(x = ximg->width; x; x--){
			unsigned int u_ratio = (bufx & 1023);

			*newBuf++ = GETLVAL(bufx >> 10, bufy >> 10, u_ratio, v_ratio, 2);
			*newBuf++ = GETLVAL(bufx >> 10, bufy >> 10, u_ratio, v_ratio, 1);
			*newBuf++ = GETLVAL(bufx >> 10, bufy >> 10, u_ratio, v_ratio, 0);
			newBuf++;
			
			bufx += dx;
		}
		newBuf += jdy;
	}

	gettimeofday(&t1, NULL);
	printf("%i ms\n", ((t1.tv_sec - t0.tv_sec) * 1000000 + t1.tv_usec - t0.tv_usec) / 1000);
}


void linearscale(struct image *img, XImage *ximg){
	unsigned int i;
	int x, y;
	unsigned int dx;
	unsigned char * __restrict__ ibuf;
	char* __restrict__ newBuf = ximg->data;
	unsigned int jdy = ximg->bytes_per_line / 4 - ximg->width;
	dx = (img->bufwidth << 10) / ximg->width;

	struct timeval t0;
	struct timeval t1;
	gettimeofday(&t0, NULL);

	for(y = 0; y < ximg->height; y++){
		i = 0;
		ibuf = &img->buf[y * img->bufheight / ximg->height * img->bufwidth * 3];
		for(x = 0; x < ximg->width; x++){
			*newBuf++ = (ibuf[(i >> 10)*3+2]);
			*newBuf++ = (ibuf[(i >> 10)*3+1]);
			*newBuf++ = (ibuf[(i >> 10)*3]);
			newBuf++;
			
			i += dx;
		}
		newBuf += jdy;
	}

	gettimeofday(&t1, NULL);
	printf("%i ms\n", ((t1.tv_sec - t0.tv_sec) * 1000000 + t1.tv_usec - t0.tv_usec) / 1000);
}

XShmSegmentInfo *shminfo;
XImage *ximage(struct image *img, unsigned int width, unsigned int height) {
	int depth;
	XImage *ximg = NULL;
	Visual *vis;

	depth = DefaultDepth(display, screen);
	vis = DefaultVisual(display, screen);

	if(depth >= 24){
		if(xshm){
			shminfo = malloc(sizeof(XShmSegmentInfo));
			if(!(ximg = XShmCreateImage(display,
				CopyFromParent, depth,
				ZPixmap,
				NULL,
				shminfo,
				width, height
			))){
			}
			if((shminfo->shmid = shmget(IPC_PRIVATE, ximg->bytes_per_line * ximg->height, IPC_CREAT|0777)) == -1){
				fprintf(stderr, "XShm problems\n");
				exit(1);
			}
			if((shminfo->shmaddr = ximg->data = shmat(shminfo->shmid, 0, 0)) == (void *)-1){
				fprintf(stderr, "XShm problems\n");
				exit(1);
			}
			shminfo->readOnly = False;
			if(!XShmAttach(display, shminfo)){
				fprintf(stderr, "XShm problems, falling back to to XImage\n");
				xshm = 0;
			}
		}
		if(!xshm){
			ximg = XCreateImage(display, 
				CopyFromParent, depth, 
				ZPixmap, 0, 
				NULL,
				width, height,
				32, 0
			);
			ximg->data  = malloc(ximg->bytes_per_line * ximg->height + /*HACK*/(ximg->bytes_per_line+1));
			XInitImage(ximg);
		}
		scale(img, ximg);
	}else{
		/* TODO other depths */
		fprintf(stderr, "This program does not yet support display depths <24.\n");
		exit(1);
		return NULL;				
	}

	return ximg;
}

XImage *getimage(struct image *img, unsigned int width, unsigned int height){
	if(width * img->bufheight > height * img->bufwidth){
		return ximage(img, img->bufwidth * height / img->bufheight, height);
	}else{
		return ximage(img, width, img->bufheight * width / img->bufwidth);
	}
}

void drawimage(XImage *ximg, unsigned int width, unsigned int height){
	XRectangle rects[2];
	int yoffset, xoffset;
	xoffset = (width - ximg->width) / 2;
	yoffset = (height - ximg->height) / 2;
	if(xoffset || yoffset){
		rects[0].x = rects[0].y = 0;
		if(xoffset){
			rects[0].width = rects[1].width = xoffset;
			rects[0].height = rects[1].height = height;
			rects[1].x = width - xoffset;
			rects[1].y = 0;
		}else if(yoffset){
			rects[0].width = rects[1].width = width;
			rects[0].height = rects[1].height = yoffset;
			rects[1].x = 0;
			rects[1].y = height - yoffset;
		}
		XFillRectangles(display, window, gc, rects, 2);
	}
	if(xshm){
		XShmPutImage(display, window, gc, ximg, 0, 0, xoffset, yoffset, ximg->width, ximg->height, False);
	}else{
		XPutImage(display, window, gc, ximg, 0, 0, xoffset, yoffset, ximg->width, ximg->height);
	}
	XFlush(display);
}

void freeXImg(XImage *ximg){
	if(xshm){
		XShmDetach(display, shminfo);
		XDestroyImage(ximg);
		shmdt(shminfo->shmaddr);
		shmctl(shminfo->shmid, IPC_RMID, 0);
		free(shminfo);
	}else{
		XDestroyImage(ximg);
	}
}


void setaspect(unsigned int w, unsigned int h){
	XSizeHints *hints = XAllocSizeHints();
	//hints->flags = PAspect;
	hints->flags = 0;
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



