
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

struct jpeg_t{
	struct image img;
	FILE *f;
	struct jpeg_decompress_struct cinfo;
	struct error_mgr jerr;
};

static void error_exit(j_common_ptr cinfo){
	(void) cinfo;
	printf("\nerror!\n");
	exit(1);
}

static void error_longjmp(j_common_ptr cinfo){
	struct error_mgr *myerr = (struct error_mgr *)cinfo->err;
	(*cinfo->err->output_message)(cinfo);
	longjmp(myerr->jmp_buffer, 1);
}

/* TODO progressive */
static struct image *jpeg_open(FILE *f){
	struct jpeg_t *j;

	rewind(f);
	if(getc(f) != 0xff || getc(f) != 0xd8)
		return NULL;

	j = malloc(sizeof(struct jpeg_t));
	j->f = f;

	j->cinfo.err = jpeg_std_error(&j->jerr.pub);
	j->jerr.pub.error_exit = error_longjmp;
	if (setjmp(j->jerr.jmp_buffer)) {
		return NULL;
	}

	j->jerr.pub.error_exit = error_exit;

	j->img.fmt = &libjpeg;

	return (struct image *)j;
}

void jpeg_prep(struct image *img){
	struct jpeg_t *j = (struct jpeg_t *)img;
	
	jpeg_create_decompress(&j->cinfo);
	rewind(j->f);
	jpeg_stdio_src(&j->cinfo, j->f);
	jpeg_read_header(&j->cinfo, TRUE);

	/* parameters */
	j->cinfo.do_fancy_upsampling = 0;
	j->cinfo.do_block_smoothing = 0;
	j->cinfo.quantize_colors = 0;
	j->cinfo.dct_method = JDCT_FASTEST;
	j->cinfo.scale_denom = img->state < FASTLOADED ? 8 : 1; /* TODO: This should be changed done only for large jpegs */

	jpeg_calc_output_dimensions(&j->cinfo);

	j->img.bufwidth = j->cinfo.output_width;
	j->img.bufheight = j->cinfo.output_height;
}


static int jpeg_read(struct image *img){
	struct jpeg_t *j = (struct jpeg_t *)img;
	unsigned int row_stride;
	int a = 0, b;
	unsigned int x, y;

	j->jerr.pub.error_exit = error_longjmp;
	if(setjmp(j->jerr.jmp_buffer)){
		return 1; /* ERROR */
	}

	row_stride = j->cinfo.output_width * j->cinfo.output_components;

	jpeg_start_decompress(&j->cinfo);


	if(j->cinfo.output_components == 3){
		JSAMPROW rows[2];
		rows[0] = img->buf;
		rows[1] = rows[0] + row_stride;
		for(y = 0; y < j->cinfo.output_height;){
			int n = jpeg_read_scanlines(&j->cinfo, rows, 2);
			y += n;
			rows[0] = rows[n-1] + row_stride;
			rows[1] = rows[0] + row_stride;
		}
	}else if(j->cinfo.output_components == 1){
		JSAMPARRAY buffer = (*j->cinfo.mem->alloc_sarray)((j_common_ptr)&j->cinfo, JPOOL_IMAGE, row_stride, 4);
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
	}else if(j->cinfo.output_components == 4){
		JSAMPARRAY buffer = (*j->cinfo.mem->alloc_sarray)((j_common_ptr)&j->cinfo, JPOOL_IMAGE, row_stride, 4);
		for(y = 0; y < j->cinfo.output_height; ){
			int n = jpeg_read_scanlines(&j->cinfo, buffer, 4);
			for(b = 0; b < n; b++){
				for(x = 0; x < j->cinfo.output_width; x++){
					int tmp = buffer[b][x*4 + 3];
					img->buf[a++] = buffer[b][x*4] * tmp / 255;
					img->buf[a++] = buffer[b][x*4 + 1] * tmp / 255;
					img->buf[a++] = buffer[b][x*4 + 2] * tmp / 255;
				}
			}
			y += n;
		}
	}else{
		fprintf(stderr, "Unsupported number of output components: %u\n", j->cinfo.output_components);
		return 1;
	}
	jpeg_finish_decompress(&j->cinfo);

	img->state = j->cinfo.scale_denom == 1 ? LOADED : FASTLOADED;

	return 0;
}

void jpeg_close(struct image *img){
	struct jpeg_t *j = (struct jpeg_t *)img;
	jpeg_destroy_decompress(&j->cinfo);
	fclose(j->f);
}

struct imageformat libjpeg = {
	jpeg_open,
	jpeg_prep,
	jpeg_read,
	jpeg_close
};

