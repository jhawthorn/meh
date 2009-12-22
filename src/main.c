
#define _GNU_SOURCE
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
struct imageformat *formats[] = {
	&libjpeg,
	&bmp,
	&libpng,
	&netpbm,
	&giflib, /* HACK! make gif last (uses read()) */
	&imagemagick,
	NULL
};


void usage(){
	printf("USAGE: meh [FILE1 [FILE2 [...]]]\n");
	printf("       meh -list [LISTFILE]      : Treat file as list of images. Defaults to stdin.\n");
	printf("       meh -ctl                  : Display files as they are received on stdin\n");
	printf("       meh -v                    : Print version and exit.\n");
	exit(EXIT_FAILURE);
}

struct image *newimage(FILE *f){
	struct image *img = NULL;
	struct imageformat **fmt = formats;
	for(fmt = formats; *fmt; fmt++){
		if((img = (*fmt)->open(f))){
			img->ximg = NULL;
			img->state = NONE;
			return img;
		}
	}
	return NULL;
}

struct image *imgopen(FILE *f){
	return newimage(f);
}

/* For MODE_CTL */
int ctlfd;

static int imageslen;
static int imageidx;
static char **images;

static int width = 0, height = 0;
static struct image *curimg = NULL;

void freeimage(struct image *img){
	if(img){
		if(img->ximg)
			freeXImg(img->ximg);
		if(img->buf)
			free(img->buf);
		free(img);
	}
}

void nextimage(){
	if(++imageidx == imageslen)
		imageidx = 0;
}

void previmage(){
	if(--imageidx < 0)
		imageidx = imageslen - 1;
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
			puts(images[imageidx]);
			fflush(stdout);
			break;
		case XK_j:
		case XK_k:
			if(mode == MODE_CTL)
				return;
			direction = key == XK_j ? nextimage : previmage;
			direction();
			/* Pass through */
		case XK_r:
			freeimage(curimg);
			curimg = NULL;
			break;
	}
}

void handleevent(XEvent *event){
	switch(event->type){
		case ConfigureNotify:
			if(width != event->xconfigure.width || height != event->xconfigure.height){
				width = event->xconfigure.width;
				height = event->xconfigure.height;
				if(curimg){
					if(curimg->ximg)
						XDestroyImage(curimg->ximg);
					curimg->ximg = NULL;
					if(curimg->state >= LOADED)
						curimg->state = LOADED;
					else if(curimg->state >= FASTLOADED)
						curimg->state = FASTLOADED;
				}

				/* Some window managers need reminding */
				if(curimg)
					setaspect(curimg->bufwidth, curimg->bufheight);
			}
			break;
		case Expose:
			if(curimg){
				if(curimg->state >= SCALED)
					curimg->state = SCALED;
				else if(curimg->state >= LOADED)
					curimg->state = LOADED;
				else if(curimg->state >= FASTSCALED)
					curimg->state = FASTSCALED;
			}
			break;
		case KeyPress:
			if(mode == MODE_CTL)
				break;
			handlekeypress(event);
			break;
	}
}

struct image *imageopen2(char *filename){
	struct image *i;
	FILE *f;
	if((f = fopen(filename, "rb"))){
		if((i = imgopen(f)))
			return i;
		else
			fprintf(stderr, "Invalid format '%s'\n", filename);
	}else{
		fprintf(stderr, "Cannot open '%s'\n", filename);
	}
	return NULL;
}

int doredraw(struct image **i){
	if(!*i){
		if(mode == MODE_CTL){
			if(images[0] == NULL)
				return 0;
			if(!(*i = imageopen2(images[0]))){
				images[0] = NULL;
				return 0;
			}
		}else{
			int firstimg = imageidx;
			while(!*i){
				if((*i = imageopen2(images[imageidx]))){
					break;
				}
				direction();
				if(imageidx == firstimg){
					fprintf(stderr, "No valid images to view\n");
					exit(EXIT_FAILURE);
				}
			}
		}

		(*i)->buf = NULL;
		(*i)->state = NONE;
	}else{
		imgstate dstate = (*i)->state + 1;
		if(dstate == LOADED || dstate == FASTLOADED){
			if((*i)->fmt->prep){
				(*i)->fmt->prep(*i);
			}
			setaspect((*i)->bufwidth, (*i)->bufheight);

			if((*i)->buf)
				free((*i)->buf);
			(*i)->buf = malloc(3 * (*i)->bufwidth * (*i)->bufheight);

			TDEBUG_START
			if((*i)->fmt->read(*i)){
				fprintf(stderr, "read error!\n");
			}
			TDEBUG_END("read");

			if((*i)->state == LOADED){
				/* We are done with the format's methods */
				(*i)->fmt->close(*i);
			}

			/* state should be set by format */
			assert((*i)->state == LOADED || (*i)->state == FASTLOADED);
		}else if(width && height){
			if(dstate == SCALED || dstate == FASTSCALED){
				(*i)->ximg = getimage(*i, width, height, dstate == FASTSCALED);
				(*i)->state = dstate;
			}else if(dstate == DRAWN || dstate == FASTDRAWN){
				assert((*i)->ximg);
				drawimage((*i)->ximg, width, height);
				(*i)->state = dstate;
			}
		}
	}
	return (*i)->state != DRAWN;
}

void run(){
	int xfd;
	xfd = ConnectionNumber(display);
	fd_set fds;
	struct timeval tv0 = {0, 0};
	struct timeval *tv = &tv0;
	int maxfd = xfd > ctlfd ? xfd : ctlfd;

	for(;;){
		FD_ZERO(&fds);
		FD_SET(xfd, &fds);
		if(mode == MODE_CTL)
			FD_SET(ctlfd, &fds); /* STDIN */
		int ret = select(maxfd+1, &fds, NULL, NULL, tv);
		if(ret == -1){
			perror("select failed\n");
		}
		if(FD_ISSET(ctlfd, &fds)){
			assert(mode == MODE_CTL);
			size_t slen = 0;
			ssize_t read;
			if(images[0]){
				free(images[0]);
			}
			freeimage(curimg);
			curimg = NULL;
			images[0] = NULL;
			if((read = getline(images, &slen, stdin)) == -1){
				exit(EXIT_SUCCESS);
			}
			images[0][read-1] = '\0';
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
			if(mode == MODE_CTL && images[0] == NULL){
				tv = NULL;
			}else if(!doredraw(&curimg)){
				/* If we get here  everything has been drawn in full or we need more input to continue */
				tv = NULL;
			}
		}
	}
}

void readlist(FILE *f){
	int lsize = 16;
	imageslen = 0;
	images = NULL;
	while(!feof(f)){
		images = realloc(images, lsize * sizeof(char *));
		while(imageslen < lsize && !feof(f)){
			char *line = NULL;
			size_t slen = 0;
			ssize_t read;
			read = getline(&line, &slen, f);
			if(read > 1){
				line[read-1] = '\0';
				images[imageslen++] = line;
			}else if(line){
				free(line);
			}
		}
		lsize *= 2;
	}
	if(!imageslen){
		fprintf(stderr, "No images to view\n");
		exit(EXIT_FAILURE);
	}
}

int main(int argc, char *argv[]){
	if(argc < 2)
		usage();

	if(!strcmp(argv[1], "-ctl")){
		if(argc != 2)
			usage();
		mode = MODE_CTL;
		images = malloc(sizeof(char *));
		images[0] = NULL;
		imageslen = 1;
		imageidx = 0;
		ctlfd = 0;
	}else if(!strcmp(argv[1], "-list")){
		mode = MODE_LIST;
		if(argc == 2){
			readlist(stdin);
		}else if(argc == 3){
			FILE *f = fopen(argv[2], "r");
			readlist(f);
			fclose(f);
		}else{
			usage();
		}
	}else if(!strcmp(argv[1], "-v")){
		printf("meh version 0.2\n");
		return 0;
	}else{
		mode = MODE_NORM;
		images = &argv[1];
		imageslen = argc-1;
		imageidx = 0;
	}
	xinit();
	run();

	return 0;
}


