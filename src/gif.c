
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

#include <assert.h>

#include <gif_lib.h>

#include "meh.h"

struct gif_t{
	struct image img;
	FILE *f;
	GifFileType *gif;
};

static struct image *gif_open(FILE *f){
	struct gif_t *g;
	GifFileType *gif;

	lseek(fileno(f), 0L, SEEK_SET);
	if(!(gif = DGifOpenFileHandle(fileno(f)))){
		return NULL;
	}
	g = malloc(sizeof(struct gif_t));
	g->f = f;
	g->gif = gif;

	g->img.width = gif->SWidth;
	g->img.height = gif->SHeight;

	return (struct image *)g;
}

static int gif_read(struct image *img){
	int i, j = 0;
	struct gif_t *g = (struct gif_t *)img;
	GifColorType *colormap;
	SavedImage *s;

	if(DGifSlurp(g->gif) == GIF_ERROR){
		PrintGifError();
		return 1;
	}

	s = &g->gif->SavedImages[0];

	if(s->ImageDesc.ColorMap)
		colormap = s->ImageDesc.ColorMap->Colors;
	else if(g->gif->SColorMap)
		colormap = g->gif->SColorMap->Colors;
	else{
		PrintGifError();
		return 1;
	}

	for(i = 0; i < img->width * img->height; i++){
		unsigned char idx = s->RasterBits[i];
		img->buf[j++] = colormap[idx].Red;
		img->buf[j++] = colormap[idx].Green;
		img->buf[j++] = colormap[idx].Blue;
	}

	DGifCloseFile(g->gif);

	return 0;
}

struct imageformat giflib = {
	gif_open,
	gif_read
};

