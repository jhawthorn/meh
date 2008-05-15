
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "jpeglib.h"

#include "jpeg.h"

unsigned char *loadjpeg(char *filename, int *width, int *height){
	struct jpeg_decompress_struct cinfo;
	struct jpeg_error_mgr jerr;

	FILE *infile;
	JSAMPARRAY buffer;
	int row_stride;
	int i = 0;
	int x, y;
	unsigned char *retbuf;

	if((infile = fopen(filename, "rb")) == NULL){
		fprintf(stderr, "can't open %s\n", filename);
		exit(1);
	}

	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_decompress(&cinfo);
	jpeg_stdio_src(&cinfo, infile);
	jpeg_read_header(&cinfo, TRUE);
	jpeg_start_decompress(&cinfo);
	*width = cinfo.output_width;
	*height = cinfo.output_height;
	retbuf = malloc(cinfo.output_components * (cinfo.output_width * cinfo.output_height));

	if(cinfo.output_components != 3){
		fprintf(stderr, "TODO: greyscale images are not supported\n");
		exit(1);
	}
	row_stride = cinfo.output_width * cinfo.output_components;
	buffer = (*cinfo.mem->alloc_sarray)((j_common_ptr) &cinfo, JPOOL_IMAGE, row_stride, 1);
	for(y = 0; y < cinfo.output_height; y++){
		jpeg_read_scanlines(&cinfo, buffer, 1);
		for(x = 0; x < row_stride; x++){
			retbuf[i++] = buffer[0][x];
		}
		assert(i % *width == 0);
	}
	jpeg_finish_decompress(&cinfo);
	jpeg_destroy_decompress(&cinfo);
	fclose(infile);
	return retbuf;
}


