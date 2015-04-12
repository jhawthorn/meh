
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

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
	if(fscanf(f, "%u", &b->img.bufwidth) != 1) goto err;
	skipspace(f);
	if(fscanf(f, "%u", &b->img.bufheight) != 1) goto err;
	if(format == '1' || format == '4'){
		b->maxval = 1;
	}else{
		skipspace(f);
		if(fscanf(f, "%u", &b->maxval) != 1) goto err;
	}

	/* whitespace character */
	fgetc(f);

	b->f = f;
	b->img.fmt = &netpbm;

	return (struct image *)b;
	
err:
	free(b);
	return NULL;		
}

static unsigned char readvali(struct netpbm_t *b){
	skipspace(b->f);
	int val;
	int unused = fscanf(b->f, "%i", &val);
	return val * 255 / b->maxval;
}

static unsigned char readvalb(struct netpbm_t *b){
	if(b->maxval == 65535){
		int val = fgetc(b->f) << 8;
		val |= fgetc(b->f);
		return val * 255 / b->maxval;
	}else{
		int val = fgetc(b->f);
		return val * 255 / b->maxval;
	}
}

int netpbm_read(struct image *img){
	struct netpbm_t *b = (struct netpbm_t *)img;
	FILE *f = b->f;
	int a = 0;

	int left = img->bufwidth * img->bufheight;
	int j, c, val;

	if(b->format == '1'){
		while(left--){
			skipspace(f);
			val = fgetc(f);
			val = val == '1' ? 0 : 255;
			img->buf[a++] = val;
			img->buf[a++] = val;
			img->buf[a++] = val;
		}
	}else if(b->format == '2'){
		while(left--){
			val = readvali(b);
			img->buf[a++] = val;
			img->buf[a++] = val;
			img->buf[a++] = val;
		}
	}else if(b->format == '3'){
		while(left--){
			img->buf[a++] = readvali(b);
			img->buf[a++] = readvali(b);
			img->buf[a++] = readvali(b);
		}
	}else if(b->format == '4'){
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
	}else if(b->format == '5'){
		while(left--){
			val = readvalb(b);
			img->buf[a++] = val;
			img->buf[a++] = val;
			img->buf[a++] = val;
		}
	}else if(b->format == '6'){
		if(b->maxval == 255){
			int unused = fread(img->buf, 1, left * 3, f);
		}else{
			while(left--){
				img->buf[a++] = readvalb(b);
				img->buf[a++] = readvalb(b);
				img->buf[a++] = readvalb(b);
			}
		}
	}

	img->state |= LOADED | SLOWLOADED;
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

