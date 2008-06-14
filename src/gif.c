
#include <stdio.h>
#include <string.h>

#include <assert.h>

#include <gif_lib.h>

#include "meh.h"


unsigned char *loadgif(FILE *f, int *bufwidth, int *bufheight){
	GifFileType *gif;
	SavedImage *img;
	GifColorType *colormap;

	unsigned char *buf;
	if(!(gif = DGifOpenFileHandle(fileno(f)))){
		return NULL;
	}
	if(DGifSlurp(gif) == GIF_ERROR){
		return NULL;
	}
	img = &gif->SavedImages[0];
	*bufwidth = img->ImageDesc.Width;
	*bufheight = img->ImageDesc.Height;
	buf = malloc(img->ImageDesc.Width * img->ImageDesc.Height * 3);

	if(img->ImageDesc.ColorMap)
		colormap = img->ImageDesc.ColorMap->Colors;
	else if(gif->SColorMap)
		colormap = gif->SColorMap->Colors;
	else
		return NULL;

	int i, j = 0;
	for(i = 0; i < img->ImageDesc.Width * img->ImageDesc.Height; i++){
		unsigned char idx = img->RasterBits[i];
		buf[j++] = colormap[idx].Red;
		buf[j++] = colormap[idx].Green;
		buf[j++] = colormap[idx].Blue;
	}
	return buf;
}

