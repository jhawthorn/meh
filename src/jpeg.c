
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <setjmp.h>
#include <string.h>

#include "jpeglib.h"

#include "meh.h"

struct error_mgr{
	struct jpeg_error_mgr pub;
	jmp_buf jmp_buffer;
};

static void error_exit(j_common_ptr cinfo){
	(void) cinfo;
	printf("\nerror!\n");
	exit(1);
}

struct jpeg_t{
	struct image img;
	FILE *f;
	struct jpeg_decompress_struct cinfo;
	struct error_mgr jerr;
};

static struct image *jpeg_open(FILE *f){
	struct jpeg_t *j;

	rewind(f);
	if(getc(f) != 0xff || getc(f) != 0xd8)
		return NULL;

	j = malloc(sizeof(struct jpeg_t));
	j->f = f;
	rewind(f);

	j->cinfo.err = jpeg_std_error(&j->jerr.pub);
	j->jerr.pub.error_exit = error_exit;

	jpeg_create_decompress(&j->cinfo);
	jpeg_stdio_src(&j->cinfo, f);
	jpeg_read_header(&j->cinfo, TRUE);

	/* parameters */
	j->cinfo.do_fancy_upsampling = 0;
	j->cinfo.do_block_smoothing = 0;
	j->cinfo.quantize_colors = 0;
	j->cinfo.dct_method = JDCT_IFAST;

	jpeg_calc_output_dimensions(&j->cinfo);

	j->img.width = j->cinfo.output_width;
	j->img.height = j->cinfo.output_height;

	return (struct image *)j;
}

static int jpeg_read(struct image *img){
	struct jpeg_t *j = (struct jpeg_t *)img;
	JSAMPARRAY buffer;
	int row_stride;
	int a = 0, b;
	unsigned int x, y;

	row_stride = j->cinfo.output_width * j->cinfo.output_components;
	buffer = (*j->cinfo.mem->alloc_sarray)((j_common_ptr)&j->cinfo, JPOOL_IMAGE, row_stride, 4);

	jpeg_start_decompress(&j->cinfo);

	if(j->cinfo.output_components == 3){
		for(y = 0; y < j->cinfo.output_height; ){
			int n = jpeg_read_scanlines(&j->cinfo, buffer, 4);
			for(b = 0; b < n; b++){
				memcpy(&img->buf[a], buffer[b], row_stride);
				a += row_stride;
			}
			y += n;
		}
	}else if(j->cinfo.output_components == 1){
		for(y = 0; y < j->cinfo.output_height; ){
			int n = jpeg_read_scanlines(&j->cinfo, buffer, 4);
			for(b = 0; b < n; b++){
				for(x = 0; x < j->cinfo.output_width; x++){
					img->buf[a++] = buffer[b][x];
					img->buf[a++] = buffer[b][x];
					img->buf[a++] = buffer[b][x];
				}
			}
			y += n;
		}
	}
	jpeg_finish_decompress(&j->cinfo);
	jpeg_destroy_decompress(&j->cinfo);

	return 0;
}

struct imageformat jpeg = {
	jpeg_open,
	jpeg_read
};

