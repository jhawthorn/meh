#include <png.h>

#include "meh.h"

unsigned char *loadpng(FILE *infile, int *bufwidth, int *bufheight){
	png_structp png_ptr;
	png_infop info_ptr;
	png_infop end_info;
	int numpasses = 1;
	unsigned char *buf;
	
	if((png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL)) == NULL)
		return NULL;
	if((info_ptr = png_create_info_struct(png_ptr)) == NULL){
		png_destroy_read_struct(&png_ptr, (png_infopp)NULL, (png_infopp)NULL);
		return NULL;
	}
	if((end_info = png_create_info_struct(png_ptr)) == NULL){
		png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);
		return NULL;
	}
	if(setjmp(png_jmpbuf(png_ptr))){
		png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
		return NULL;
	}

	png_init_io(png_ptr, infile);

	/* Start reading */
	png_read_info(png_ptr, info_ptr);
	buf = malloc(100);
	{
		png_byte color_type = png_get_color_type(png_ptr, info_ptr);
		if(color_type == PNG_COLOR_TYPE_PALETTE)
			png_set_palette_to_rgb(png_ptr);
		if(color_type == PNG_COLOR_TYPE_GRAY && png_get_bit_depth(png_ptr, info_ptr) < 8)
			png_set_gray_1_2_4_to_8(png_ptr);
		if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
			png_set_tRNS_to_alpha(png_ptr);
		if (png_get_bit_depth(png_ptr, info_ptr) == 16)
			png_set_strip_16(png_ptr);
		if(color_type & PNG_COLOR_MASK_ALPHA)
			png_set_strip_alpha(png_ptr);
		if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
			png_set_gray_to_rgb(png_ptr);
		if(png_get_interlace_type == PNG_INTERLACE_ADAM7)
			numpasses = png_set_interlace_handling(png_ptr);
		png_read_update_info(png_ptr, info_ptr);
	}
	{
		int y;
		*bufwidth = png_get_image_width(png_ptr, info_ptr);
		*bufheight = png_get_image_height(png_ptr, info_ptr);
		buf = malloc((*bufwidth) * (*bufheight) * 3);
		printf("%ix%i\n", *bufwidth, *bufheight);
		while(numpasses--){
			for(y = 0; y < *bufheight; y++)
				png_read_row(png_ptr, &buf[y * (*bufwidth) * 3], NULL);
		}
	}

	printf("reached end\n");
	return buf;
}

