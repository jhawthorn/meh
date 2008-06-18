
#include <stdio.h>
#include <stdlib.h>

#include "meh.h"

unsigned short getshort(FILE *f){
	unsigned short ret;
	ret = getc(f);
	ret = ret | (getc(f) << 8);
	return ret;
}

unsigned long getlong(FILE *f){
	unsigned short ret;
	ret = getc(f);
	ret = ret | (getc(f) << 8);
	ret = ret | (getc(f) << 16);
	ret = ret | (getc(f) << 24);
	return ret;
}

struct rgb_t{
	unsigned char r, g, b;
};

struct bmp_t{
	struct image img;
	unsigned long bitmapoffset;
	int bpp;
	int ncolors;
	struct rgb_t *colours;
};

struct image *bmp_open(FILE *f){
	struct bmp_t *b;
	unsigned long headerend;
	
	rewind(f);
	if(getc(f) != 'B' || getc(f) != 'M')
		return NULL;

	b = malloc(sizeof(struct bmp_t));
	b->img.f = f;

	fseek(f, 10, SEEK_SET);
	b->bitmapoffset = getlong(f);

	fseek(f, 18, SEEK_SET);
	b->img.width = getlong(f);
	b->img.height = getlong(f);

	fseek(f, 14, SEEK_SET);
	headerend = 14 + getlong(f);

	fseek(f, 28, SEEK_SET);
	b->bpp = getshort(f);

	if(b->bpp == 24){
		b->ncolors = 0;
	}else{
		int i;
		fseek(f, 46, SEEK_SET);
		if(!(b->ncolors = getlong(f))){
			b->ncolors = 1 << b->bpp;
		}
		
		b->colours = malloc(b->ncolors * sizeof(struct rgb_t));
		fseek(f, headerend, SEEK_SET);
		for(i = 0; i < b->ncolors; i++){
			b->colours[i].b = getc(f);
			b->colours[i].g = getc(f);
			b->colours[i].r = getc(f);
		}
	}

	return (struct image *)b;
}

int bmp_read(struct image *img){
	struct bmp_t *b = (struct bmp_t *)img;
	FILE *f = img->f;

	fseek(f, b->bitmapoffset, SEEK_SET);
	if(b->bpp == 24){
		int i, x, y;
		int row = img->width * 3;
		i = img->height * row;
		for(y = img->height; y; y--){
			i -= row;
			for(x = 0; x < img->width * 3; x+=3){
				img->buf[i + x + 2] = getc(f);
				img->buf[i + x + 1] = getc(f);
				img->buf[i + x + 0] = getc(f);
			}
			if(x & 3){
				fseek(f, 4 - (x & 3), SEEK_CUR);
			}
		}
	}else{
		//TODO
	}

	return 0;
}

struct imageformat gif = {
	bmp_open,
	bmp_read
};

