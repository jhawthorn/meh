
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <setjmp.h>
#include <string.h>

#include "jpeglib.h"

#include "jpeg.h"

struct error_mgr{
	struct jpeg_error_mgr pub;
	jmp_buf jmp_buffer;
};

static void error_exit(j_common_ptr cinfo){
	printf("\nerror!\n");
	exit(1);
}

unsigned char *loadjpeg(FILE *infile, int *width, int *height){
	struct jpeg_decompress_struct cinfo;
	struct error_mgr jerr;

	JSAMPARRAY buffer;
	int row_stride;
	int i = 0;
	int j;
	int y;
	unsigned char *retbuf;

	cinfo.err = jpeg_std_error(&jerr.pub);
	jerr.pub.error_exit = error_exit;

	jpeg_create_decompress(&cinfo);
	jpeg_stdio_src(&cinfo, infile);
	jpeg_read_header(&cinfo, TRUE);

	/* parameters */
	cinfo.do_fancy_upsampling = 0;
	cinfo.do_block_smoothing = 0;
	cinfo.quantize_colors = 0;
	cinfo.dct_method = JDCT_IFAST;
	
	jpeg_calc_output_dimensions(&cinfo);
	row_stride = cinfo.output_width * cinfo.output_components;
	buffer = (*cinfo.mem->alloc_sarray)((j_common_ptr)&cinfo, JPOOL_IMAGE, row_stride, 4);
	
	jpeg_start_decompress(&cinfo);
	*width = cinfo.output_width;
	*height = cinfo.output_height;
	retbuf = malloc(4 * cinfo.output_components * (cinfo.output_width * cinfo.output_height));

	if(cinfo.output_components != 3){
		fprintf(stderr, "TODO: greyscale images are not supported\n");
		exit(1);
	}
	for(y = 0; y < cinfo.output_height; ){
		int n = jpeg_read_scanlines(&cinfo, buffer, 4);
		for(j = 0; j < n; j++){
			memcpy(&retbuf[i], buffer[j], row_stride);
			i += row_stride;
		}
		y += n;
	}
	jpeg_finish_decompress(&cinfo);
	jpeg_destroy_decompress(&cinfo);
	return retbuf;
}


