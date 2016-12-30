
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/extensions/XShm.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "meh.h"
#include "scale.h"

struct data_t{
	XImage *ximg;
	XShmSegmentInfo *shminfo;
};

/* Globals */
static Display *display;
static int screen;
static Window window;
static GC gc;

static int xfd;

static int xshm = 0;

static void ximage(struct data_t *data, struct image *img, unsigned int width, unsigned int height, int fast){
	int depth;
	XImage *ximg = NULL;
	XShmSegmentInfo *shminfo = NULL;

	depth = DefaultDepth(display, screen);

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
				fprintf(stderr, "XShm problems\n");
				exit(1);
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
		}else{
			ximg = XCreateImage(display, 
				CopyFromParent, depth, 
				ZPixmap, 0, 
				NULL,
				width, height,
				32, 0
			);
			ximg->data = malloc(ximg->bytes_per_line * ximg->height);
			XInitImage(ximg);
		}
		(fast ? nearestscale : scale)(img, ximg->width, ximg->height, ximg->bytes_per_line, ximg->data);
	}else{
		/* TODO other depths */
		fprintf(stderr, "This program does not yet support display depths <24.\n");
		exit(1);
	}

	data->ximg = ximg;
	data->shminfo = shminfo;
}

void backend_prepare(struct image *img, unsigned int width, unsigned int height, int fast){
	struct data_t *data = img->backend = malloc(sizeof (struct data_t));
	if(width * img->bufheight > height * img->bufwidth){
		ximage(data, img, img->bufwidth * height / img->bufheight, height, fast);
	}else{
		ximage(data, img, width, img->bufheight * width / img->bufwidth, fast);
	}

	img->state |= SCALED;
	if(!fast)
		img->state |= SLOWSCALED;
}

void backend_draw(struct image *img, unsigned int width, unsigned int height){
	assert(((struct data_t *)img->backend));
	assert(((struct data_t *)img->backend)->ximg);

	XImage *ximg = ((struct data_t *)img->backend)->ximg;

	XRectangle rects[2];
	int yoffset, xoffset;
	xoffset = (width - ximg->width) / 2;
	yoffset = (height - ximg->height) / 2;
	if(xoffset || yoffset){
		rects[0].x = rects[0].y = 0;
		if(xoffset){
			rects[0].width = rects[1].width = xoffset;
			rects[0].height = rects[1].height = height;
			rects[1].x = xoffset + ximg->width;
			rects[1].y = 0;
			rects[1].width = width - rects[1].x;
		}else if(yoffset){
			rects[0].width = rects[1].width = width;
			rects[0].height = yoffset;
			rects[1].x = 0;
			rects[1].y = yoffset + ximg->height;
			rects[1].height = height - rects[1].y;
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

void backend_free(struct image *img){
	assert(img);
	if(img->backend){
		struct data_t *data = (struct data_t *)img->backend;
		XImage *ximg = data->ximg;
		if(ximg){
			if(xshm && data->shminfo){
				XShmDetach(display, data->shminfo);
				XDestroyImage(ximg);
				shmdt(data->shminfo->shmaddr);
				shmctl(data->shminfo->shmid, IPC_RMID, 0);
				free(data->shminfo);
			}else{
				XDestroyImage(ximg);
			}
		}
		img->backend = NULL;
	}
}


void backend_setaspect(unsigned int w, unsigned int h){
	XSizeHints *hints = XAllocSizeHints();
	//hints->flags = PAspect;
	hints->flags = 0;
	hints->min_aspect.x = hints->max_aspect.x = w;
	hints->min_aspect.y = hints->max_aspect.y = h;
	XSetWMNormalHints(display, window, hints);
	XFlush(display);
	XFree(hints);
}

/* Alt-F4 silent. Keeps people happy */
static int xquit(Display *d){
	(void)d;
	exit(EXIT_SUCCESS);
	return 0;
}

void backend_init(){
	display = XOpenDisplay(NULL);
	if(!display){
		fprintf(stderr, "Can't open X display.\n");
		exit(EXIT_FAILURE);
	}
	xfd = ConnectionNumber(display);
	screen = DefaultScreen(display);

	window = XCreateWindow(display, DefaultRootWindow(display), 0, 0, 640, 480, 0, DefaultDepth(display, screen), InputOutput, CopyFromParent, 0, NULL);
	backend_setaspect(1, 1);
	gc = XCreateGC(display, window, 0, NULL);

	XSelectInput(display, window, StructureNotifyMask | ExposureMask | KeyPressMask);
	XMapRaised(display, window);
	XFlush(display);
	XSetIOErrorHandler(xquit);
	XFlush(display);
}

void handlekeypress(XEvent *event){
	KeySym key = XLookupKeysym(&event->xkey, 0);
	switch(key){
		case XK_Escape:
		case XK_q:
			meh_quit();
			break;
		case XK_Return:
			key_action();
			break;
		case XK_j:
		case XK_l:
		case XK_Right:
		case XK_Down:
			key_next();
			break;
		case XK_k:
		case XK_h:
		case XK_Left:
		case XK_Up:
			key_prev();
			break;
		case XK_r:
			key_reload();
			break;
		case XK_0:
			if(curimg)
				XResizeWindow(display, window, curimg->bufwidth, curimg->bufheight);
			break;
	}
}


void handleevent(XEvent *event){
	Atom closed = XInternAtom(display, "WM_DELETE_WINDOW", True);
	XSetWMProtocols(display, window, &closed, 1);

	switch(event->type){
		/* Might not get ConfigureNotify, for example if there's no window manager */
		case ClientMessage:
			meh_quit();
			break;
		case MapNotify:
			if (!width || !height)
			{
				XWindowAttributes attr;
				XGetWindowAttributes(event->xmap.display, event->xmap.window, &attr);
				width = attr.width;
				height = attr.height;
			}
			break;
		case ConfigureNotify:
			if(width != event->xconfigure.width || height != event->xconfigure.height){
				width = event->xconfigure.width;
				height = event->xconfigure.height;
				if(curimg){
					backend_free(curimg);
					curimg->state &= ~(DRAWN | SCALED | SLOWSCALED);

					/* Some window managers need reminding */
					backend_setaspect(curimg->bufwidth, curimg->bufheight);
				}
			}
			break;
		case Expose:
			if(curimg){
				curimg->state &= ~DRAWN;
			}
			break;
		case KeyPress:
			handlekeypress(event);
			break;
	}
}

void backend_run(){
	fd_set fds;
	struct timeval tv0 = {0, 0};
	struct timeval *tv = &tv0;

	for(;;){
		int maxfd = setup_fds(&fds);
		FD_SET(xfd, &fds);
		if(xfd > maxfd)
			maxfd = xfd;
		int ret = select(maxfd+1, &fds, NULL, NULL, tv);
		if(ret == -1){
			perror("select failed\n");
		}
		if(process_fds(&fds)){
			tv = &tv0;
		}
		if(XPending(display)){
			tv = &tv0;
			XEvent event;
			do{
				XNextEvent(display, &event);
				handleevent(&event);
			}while(XPending(display));
		}else if(ret == 0){
			if(!process_idle()){
				/* If we get here everything has been drawn in full or we need more input to continue */
				tv = NULL;
			}
		}
	}
}


