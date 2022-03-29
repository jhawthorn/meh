
#include <stdio.h>
#include <stdlib.h>

#include "meh.h"

struct rgb_t{
	unsigned char r, g, b;
};

struct bmp_t{
	struct image img;
	FILE *f;
	unsigned long bitmapoffset;
	int compression;
	int bpp;
	int ncolors;
	struct rgb_t *colours;
	unsigned int rowwidth;
};

static unsigned short getshort(FILE *f){
	unsigned short ret;
	ret = getc(f);
	ret = ret | (getc(f) << 8);
	return ret;
}

static unsigned long getlong(FILE *f){
	unsigned short ret;
	ret = getc(f);
	ret = ret | (getc(f) << 8);
	ret = ret | (getc(f) << 16);
	ret = ret | (getc(f) << 24);
	return ret;
}

struct image *bmp_open(FILE *f){
	struct bmp_t *b;
	unsigned long headersize;
	
	rewind(f);
	if(getc(f) != 'B' || getc(f) != 'M')
		return NULL;

	b = malloc(sizeof(struct bmp_t));
	b->f = f;

	fseek(f, 10, SEEK_SET);
	b->bitmapoffset = getlong(f);

	fseek(f, 14, SEEK_SET);
	headersize = getlong(f);

	if(headersize == 12){ /* OS/2 v1 */
		b->ncolors = 0;
		fseek(f, 18, SEEK_SET);
		b->img.bufwidth = getshort(f);
		b->img.bufheight = getshort(f);
		b->compression = 0;
	}else{
		fseek(f, 18, SEEK_SET);
		b->img.bufwidth = getlong(f);
		b->img.bufheight = getlong(f);

		fseek(f, 28, SEEK_SET);
		b->bpp = getshort(f);

		fseek(f, 30, SEEK_SET);
		b->compression = getlong(f);

		fseek(f, 46, SEEK_SET);
		b->ncolors = getlong(f);
	}

	if(!b->ncolors){
		b->ncolors = 1 << b->bpp;
	}

	if(b->compression){
		fprintf(stderr, "unsupported compression method %i\n", b->compression);
		free(b);
		return NULL;
	}

	if(b->bpp >= 16){
		b->rowwidth = b->img.bufwidth * b->bpp / 8;
		b->colours = NULL;
	}else{
		int i;
		b->colours = malloc(b->ncolors * sizeof(struct rgb_t));
		fseek(f, 14+headersize, SEEK_SET);
		for(i = 0; i < b->ncolors; i++){
			b->colours[i].b = getc(f);
			b->colours[i].g = getc(f);
			b->colours[i].r = getc(f);
			if(headersize != 12)
				getc(f);
		}
		b->rowwidth = (b->img.bufwidth * b->bpp + 7) / 8;
	}
	if(b->rowwidth & 3){
		b->rowwidth += 4 - (b->rowwidth & 3);
	}

	b->img.fmt = &bmp;

	return (struct image *)b;
}

static void rgb16(unsigned char *buf, unsigned short c){
	int i;
	for(i = 0; i < 3; i++){
		*buf++ = ((c >> (i * 5)) & 0x1f) << 3;
	}
}

static int readrow(struct image *img, unsigned char *row, unsigned char *buf){
	struct bmp_t *b = (struct bmp_t *)img;
	unsigned int x, i = 0;
	if(b->bpp == 24 || b->bpp == 32){
		for(x = 0; x < img->bufwidth * 3; x+=3){
			buf[x + 2] = row[i++];
			buf[x + 1] = row[i++];
			buf[x + 0] = row[i++];
			if(b->bpp == 32)
				i++;
		}
	}else if(b->bpp == 16){
		for(x = 0; x < img->bufwidth * 3; x+=3){
			unsigned short c;
			c = row[i++];
			c |= row[i++] << 8;
			rgb16(&buf[x], c);
		}
	}else if(b->bpp <= 8){
		int mask;
		int pixelsperbit = 8 / b->bpp;
		mask = ~((~0) << b->bpp);
		for(x = 0; x < img->bufwidth; x++){
			unsigned char c = ((row[i / pixelsperbit]) >> ((8 - ((i+1) % pixelsperbit) * b->bpp)) % 8) & mask;
			*buf++ = b->colours[c].r;
			*buf++ = b->colours[c].g;
			*buf++ = b->colours[c].b;
			i++;
		}
	}else{
		fprintf(stderr, "bad bpp %i\n", b->bpp);
		return 1;
	}
	return 0;
}

int bmp_read(struct image *img){
	struct bmp_t *b = (struct bmp_t *)img;
	unsigned int i, y;
	unsigned int dy;
	unsigned char *row;
	FILE *f = b->f;

	row = malloc(b->rowwidth);
	dy = img->bufwidth * 3;
	i = img->bufheight * dy;

	fseek(f, b->bitmapoffset, SEEK_SET);
	for(y = img->bufheight; y; y--){
		i -= dy;
		if(fread(row, 1, b->rowwidth, f) != b->rowwidth || readrow(img, row, &img->buf[i])){
			free(row);
			return 1;
		}
	}

	free(row);

	img->state |= LOADED | SLOWLOADED;

	return 0;
}

void bmp_close(struct image *img){
	struct bmp_t *b = (struct bmp_t *)img;
	free(b->colours);
	fclose(b->f);
}

struct imageformat bmp = {
	bmp_open,
	NULL,
	bmp_read,
	bmp_close
};

