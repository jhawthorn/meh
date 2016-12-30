
#define _GNU_SOURCE
#include <unistd.h>

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#include "meh.h"

#define MODE_NORM 0
#define MODE_LIST 1
#define MODE_CTL 2
static int mode;

/* Supported Formats */
static struct imageformat *formats[] = {
	&libjpeg,
	&bmp,
	&libpng,
	&netpbm,
	&giflib, /* HACK! make gif last (uses read()) */
	&imagemagick,
	NULL
};


static void usage(){
	printf("USAGE: meh [FILE1 [FILE2 [...]]]\n");
	printf("       meh -list [LISTFILE]      : Treat file as list of images. Defaults to stdin.\n");
	printf("       meh -ctl                  : Display files as they are received on stdin\n");
	printf("       meh -v                    : Print version and exit.\n");
	exit(EXIT_FAILURE);
}

static struct image *newimage(FILE *f){
	struct image *img = NULL;
	struct imageformat **fmt = formats;
	for(fmt = formats; *fmt; fmt++){
		if((img = (*fmt)->open(f))){
			img->state = NONE;
			img->backend = NULL;
			return img;
		}
	}
	return NULL;
}

/* For MODE_CTL */
static int ctlfd;

static int imageslen;
static int imageidx;
static char **images;

int width = 0, height = 0;
struct image *curimg = NULL;

struct image *nextimg = NULL;
struct image *previmg = NULL;

static void freeimage(struct image *img){
	if(img){
		backend_free(img);
		if(img->buf)
			free(img->buf);
		free(img);
	}
}

static int incidx(int i){
	return ++i >= imageslen ? 0 : i;
}

static int decidx(int i){
	return --i < 0 ? imageslen - 1 : i;
}

static int (*direction)(int) = incidx;

void key_reload(){
	freeimage(curimg);
	curimg = NULL;
}
void key_next(){
	if(mode != MODE_CTL){
		if(curimg)
			curimg->state &= LOADED | SLOWLOADED;
		freeimage(previmg);
		previmg = curimg;
		curimg = nextimg;
		nextimg = NULL;
		if(curimg){
			imageidx = curimg->idx;
		}else{
			imageidx = (direction = incidx)(imageidx);
		}
	}
}
void key_prev(){
	if(mode != MODE_CTL){
		if(curimg)
			curimg->state &= LOADED | SLOWLOADED;
		freeimage(nextimg);
		nextimg = curimg;
		curimg = previmg;
		previmg = NULL;
		if(curimg){
			imageidx = curimg->idx;
		}else{
			imageidx = (direction = decidx)(imageidx);
		}
	}
}
void meh_quit(){
	exit(EXIT_SUCCESS);
}

void key_action(){
	puts(images[imageidx]);
	fflush(stdout);
}

struct image *imageopen2(char *filename){
	struct image *i;
	FILE *f;
	if((f = fopen(filename, "rb"))){
		if((i = newimage(f))){
			return i;
		}
		else
			fprintf(stderr, "Invalid format '%s'\n", filename);
	}else{
		fprintf(stderr, "Cannot open '%s'\n", filename);
	}
	return NULL;
}

static int doredraw(struct image **i, int idx, int (*dir)(int), int dstates){
	if(!*i){
		if(mode == MODE_CTL){
			if(images[0] == NULL)
				return 0;
			if(!(*i = imageopen2(images[0]))){
				images[0] = NULL;
				return 0;
			}
		}else{
			int firstimg = idx;
			while(!*i){
				if((*i = imageopen2(images[idx]))){
					break;
				}
				idx = dir(idx);
				if(idx == firstimg){
					fprintf(stderr, "No valid images to view\n");
					exit(EXIT_FAILURE);
				}
			}
		}

		(*i)->idx = idx;
		(*i)->buf = NULL;
		(*i)->state = NONE;
		return 1;
	}else{
		imgstate state = (*i)->state;
		if(!(state & LOADED) || ((state & (SLOWLOADED | DRAWN)) == (DRAWN)) ){
			if((*i)->fmt->prep){
				(*i)->fmt->prep(*i);
			}
			backend_setaspect((*i)->bufwidth, (*i)->bufheight);

			if((*i)->buf)
				free((*i)->buf);
			(*i)->buf = malloc(3 * (*i)->bufwidth * (*i)->bufheight);

			TDEBUG_START
			if((*i)->fmt->read(*i)){
				fprintf(stderr, "read error!\n");
			}
			TDEBUG_END("read");

			if(((*i)->state & LOADED) && ((*i)->state & SLOWLOADED)){
				/* We are done with the format's methods */
				(*i)->fmt->close(*i);
			}

			(*i)->state &= LOADED | SLOWLOADED;

			/* state should be set by format */
			assert((*i)->state & LOADED);
			return 1;
		}else if(width && height){
			if((dstates & SCALED) && (state & LOADED) && !(state & SCALED)){
				backend_prepare(*i, width, height, !!(state & SCALED));
				(*i)->state &= LOADED | SLOWLOADED | SCALED | SLOWSCALED;

				/* state should be set by backend */
				assert((*i)->state & SCALED);

				/* state should not be drawn so that we will draw later (also assures correct return value) */
				assert(!((*i)->state & DRAWN));
			}
		}
		if((dstates & DRAWN) && ((*i)->state & SCALED) && !(state & DRAWN)){
			backend_draw(*i, width, height);
			(*i)->state |= DRAWN;
			return 1;
		}
	}
	return 0;
}

static void readlist(FILE *f){
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

int setup_fds(fd_set *fds){
	FD_ZERO(fds);
	if(mode == MODE_CTL)
		FD_SET(ctlfd, fds); /* STDIN */
	return ctlfd;
}

int process_fds(fd_set *fds){
	if(FD_ISSET(ctlfd, fds)){
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
		return 1;
	}
	return 0;
}

int process_idle(){
	if(mode == MODE_CTL && images[0] == NULL){
		return 0;
	}else{
		int ret = doredraw(&curimg, imageidx, direction, ~0);
		imageidx = curimg->idx;
		if(!ret){
			ret = doredraw(&nextimg, incidx(imageidx), incidx, LOADED | SLOWLOADED);
		}
		if(!ret){
			ret = doredraw(&previmg, decidx(imageidx), decidx, LOADED | SLOWLOADED);
		}
		return ret;
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
		printf("meh version 0.3\n");
		return 0;
	}else{
		mode = MODE_NORM;
		images = &argv[1];
		imageslen = argc-1;
		imageidx = 0;
	}
	backend_init();
	backend_run();

	return 0;
}


