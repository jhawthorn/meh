
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

static int isgif(FILE *f){
	return (getc(f) == 'G' && getc(f) == 'I' && getc(f) == 'F');
}

static struct image *gif_open(FILE *f){
	struct gif_t *g;
	GifFileType *gif;
#if defined(GIFLIB_MAJOR) && defined(GIFLIB_MINOR) && ((GIFLIB_MAJOR == 5 && GIFLIB_MINOR >= 0) || GIFLIB_MAJOR > 5)
	int err;
#endif

	rewind(f);
	if(!isgif(f))
		return NULL;

	/* HACK HACK HACK */
	rewind(f);
	lseek(fileno(f), 0L, SEEK_SET);
#if defined(GIFLIB_MAJOR) && defined(GIFLIB_MINOR) && ((GIFLIB_MAJOR == 5 && GIFLIB_MINOR >= 0) || GIFLIB_MAJOR > 5)
	if(!(gif = DGifOpenFileHandle(fileno(f),&err))){
#else
	if(!(gif = DGifOpenFileHandle(fileno(f)))){
#endif
		/* HACK AND HOPE */
		rewind(f);
		lseek(fileno(f), 0L, SEEK_SET);
#if defined(GIFLIB_MAJOR) && defined(GIFLIB_MINOR) && ((GIFLIB_MAJOR == 5 && GIFLIB_MINOR >= 0) || GIFLIB_MAJOR > 5)
		fprintf(stderr, "%s\n", GifErrorString(err));
#endif
		return NULL;
	}
	g = malloc(sizeof(struct gif_t));
	g->f = f;
	g->gif = gif;

	g->img.bufwidth = gif->SWidth;
	g->img.bufheight = gif->SHeight;

	g->img.fmt = &giflib;

	return (struct image *)g;
}

static int gif_read(struct image *img){
	unsigned int i, j = 0;
	struct gif_t *g = (struct gif_t *)img;
	GifColorType *colormap;
	SavedImage *s;

	if(DGifSlurp(g->gif) == GIF_ERROR)
		goto error;

	s = &g->gif->SavedImages[0];

	if(s->ImageDesc.ColorMap)
		colormap = s->ImageDesc.ColorMap->Colors;
	else if(g->gif->SColorMap)
		colormap = g->gif->SColorMap->Colors;
	else
		goto error;

	for(i = 0; i < img->bufwidth * img->bufheight; i++){
		unsigned char idx = s->RasterBits[i];
		img->buf[j++] = colormap[idx].Red;
		img->buf[j++] = colormap[idx].Green;
		img->buf[j++] = colormap[idx].Blue;
	}

	img->state |= LOADED | SLOWLOADED;

	return 0;
error:
#if defined(GIFLIB_MAJOR) && defined(GIFLIB_MINOR) && ((GIFLIB_MAJOR == 5 && GIFLIB_MINOR >= 0) || GIFLIB_MAJOR > 5)
	/* how to get the error here? */
#elif defined(GIFLIB_MAJOR) && defined(GIFLIB_MINOR) && ((GIFLIB_MAJOR == 4 && GIFLIB_MINOR >= 2) || GIFLIB_MAJOR > 4)
	fprintf(stderr, "%s\n", GifErrorString());
#else
	PrintGifError();
#endif
	return 1;
}

void gif_close(struct image *img){
	struct gif_t *g = (struct gif_t *)img;
	DGifCloseFile(g->gif);
	fclose(g->f);
}

struct imageformat giflib = {
	gif_open,
	NULL,
	gif_read,
	gif_close
};

