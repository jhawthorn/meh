
#include <stdio.h>
#include <stdlib.h>

#include "meh.h"

struct netpbm_t{
	struct image img;
	FILE *f;
  char format;
	unsigned int maxval;
};

void skipspace(FILE *f){
	int c;
	for(;;){
		c = fgetc(f);
		while(isspace(c))
			c = fgetc(f);
		if(c == '#')
			while(fgetc(f) != '\n');
		else
			break;
	}
	ungetc(c, f);
}

struct image *netpbm_open(FILE *f){
	struct netpbm_t *b;
	rewind(f);

	char format;
	if(fgetc(f) != 'P')
		return NULL;
	format = fgetc(f);
	if(format > '6' || format < '1')
		return NULL;

	b = malloc(sizeof(struct netpbm_t));
	b->format = format;

	skipspace(f);
	fscanf(f, "%u", &b->img.bufwidth);
	skipspace(f);
	fscanf(f, "%u", &b->img.bufheight);
	if(format == '1' || format == '4'){
		b->maxval = 1;
	}else{
		skipspace(f);
		fscanf(f, "%u", &b->maxval);
	}

	/* whitespace character */
	fgetc(f);

	b->f = f;

	return (struct image *)b;
}

int netpbm_read(struct image *img){
	struct netpbm_t *b = (struct netpbm_t *)img;
	FILE *f = b->f;
	int a = 0;

	int left = img->bufwidth * img->bufheight;
	int j, c;

	if(b->format == '4'){
		while(left){
			c = fgetc(f);
			for(j = 0; j < 8 && left; j++){
				int val = (c & 1) ? 0 : 255;
				img->buf[a++] = val;
				img->buf[a++] = val;
				img->buf[a++] = val;
				c >>= 1;
				left--;
			}
		}
	}else if(b->format == '6'){
		while(left){
			img->buf[a++] = fgetc(f);
			img->buf[a++] = fgetc(f);
			img->buf[a++] = fgetc(f);
			left--;
		}
	}

	img->state = LOADED;
	return 0;
}

void netpbm_close(struct image *img){
	struct netpbm_t *b = (struct netpbm_t *)img;
	fclose(b->f);
}

struct imageformat netpbm = {
	netpbm_open,
	NULL,
	netpbm_read,
	netpbm_close
};

