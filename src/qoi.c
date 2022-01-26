#include "meh.h"

#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <assert.h>

/* #define DEBUG */

struct qoi_header_t {
	unsigned char magic[4]; // magic bytes "qoif"
	uint32_t width; // image width in pixels (BE)
	uint32_t height; // image height in pixels (BE)
	uint8_t channels; // 3 = RGB, 4 = RGBA
	uint8_t colorspace; // 0 = sRGB with linear alpha, 1 = all channels linear
};

#define QOI_MAGIC_LEN 4
static unsigned char QOI_MAGIC[QOI_MAGIC_LEN] = "qoif";

/* 64 colors in index */
#define QOI_SIZE_OF_COLOR_INDEX 64

/* 2GB is the max file size that this implementation can safely handle. We guard
against anything larger than that, assuming the worst case with 5 bytes per
pixel, rounded down to a nice clean value. 400 million pixels ought to be
enough for anybody. */
#define QOI_PIXELS_MAX ((unsigned int)400000000)

struct qoi_rgba_t {
	uint8_t r, g, b, a;
};

struct qoi_t{
	struct image img;
	FILE *f;
	struct qoi_header_t header;
	struct qoi_rgba_t index[QOI_SIZE_OF_COLOR_INDEX];
};

#define QOI_OP_INDEX  0x00 /* 00xxxxxx */
#define QOI_OP_DIFF   0x40 /* 01xxxxxx */
#define QOI_OP_LUMA   0x80 /* 10xxxxxx */
#define QOI_OP_RUN    0xc0 /* 11xxxxxx */
#define QOI_OP_RGB    0xfe /* 11111110 */
#define QOI_OP_RGBA   0xff /* 11111111 */

#define QOI_MASK_2    0xc0 /* 11000000 */

#define QOI_CHANNELS_RGB 3 /* only 3 channels, red, green, blue */
#define QOI_CHANNELS_RGBA 4 /* 3 RGB channels, one alpha channel */

#define QOI_SRGB   0 /* sRGB, i.e. gamma scaled RGB channels and a linear alpha channel */
#define QOI_LINEAR 1 /* all channels are linear */

struct image *qoi_open(FILE *f){
	struct qoi_t *q;
	uint32_t buf;

#ifdef DEBUG
	printf("qoi_open start\n");
#endif

	rewind(f);
	q = malloc(sizeof(struct qoi_t));
	if(!q) return NULL;
	q->f = f;

	// read header
	if(fread(q->header.magic, 1, QOI_MAGIC_LEN, f) != QOI_MAGIC_LEN)
		return NULL;
	if(memcmp(q->header.magic, QOI_MAGIC, QOI_MAGIC_LEN) != 0)
		return NULL;
	if(fread(&buf, 1, 4, f) != 4)
		return NULL;
	q->header.width = ntohl(buf);
#ifdef DEBUG
	printf("width: %d\n", q->header.width);
#endif
	if(q->header.width == 0)
		return NULL;
	if(fread(&buf, 1, 4, f) != 4)
		return NULL;
	q->header.height = ntohl(buf);
#ifdef DEBUG
	printf("height: %d\n", q->header.height);
#endif
	if(q->header.height == 0)
		return NULL;
	if(fread(&q->header.channels, 1, 1, f) != 1)
		return NULL;
	if(q->header.channels<3 || q->header.channels>4)
		return NULL;
#ifdef DEBUG
	printf("channels: %d\n", q->header.channels);
#endif
	if(fread(&q->header.colorspace, 1, 1, f) != 1)
		return NULL;
#ifdef DEBUG
	printf("colorspace: %d\n", q->header.colorspace);
#endif
	if(q->header.colorspace>1)
		return NULL;
	if(q->header.height>=QOI_PIXELS_MAX/q->header.width)
		return NULL;

	q->img.bufwidth = q->header.width;
	/* we add one row to the output buffer, so we can put the sentinel there,
	 * below we make sure we don't write pixels after we have met >1 index(0)
	 */
	q->img.bufheight = q->header.height+1;

	q->img.fmt = &qoi;

	return (struct image *)q;
}

static int color_hash( uint8_t r, uint8_t g, uint8_t b, uint8_t a ){
	return ( r*3+g*5+b*7+a*11 ) % QOI_SIZE_OF_COLOR_INDEX;
}

int qoi_read(struct image *img){
	struct qoi_t *q = (struct qoi_t *)img;
	uint8_t buf[4];
	uint8_t run = 0;
	uint32_t ptr = 0;
	int nof_index0 = 0;
	struct qoi_rgba_t background;
	struct qoi_rgba_t pixel;

#ifdef DEBUG
	printf("qoi_read start\n");
#endif

	memset(q->index, 0, QOI_SIZE_OF_COLOR_INDEX*sizeof(struct qoi_rgba_t));
	background.r = 255;
	background.g = 255;
	background.b = 255;
	pixel.r = 0;
	pixel.g = 0;
	pixel.b = 0;
	pixel.a = 255;

	while(1){
		if( run > 0 ) {
			run--;
		} else {
			if(fread(buf, 1, 1, q->f) != 1) {
				if(feof(q->f)){
					fprintf(stderr, "EOF seen, premature end of image?\n");
					goto END;
				}else
					return 1;
			}
			uint8_t op = buf[0];

			if(op == QOI_OP_RGBA){
				if(fread(buf, 1, 4, q->f) != 4) return 1;
				pixel.r = buf[0];
				pixel.g = buf[1];
				pixel.b = buf[2];
				pixel.a = buf[3];
#ifdef DEBUG
				fprintf(stderr, "QOI_OP_RGBA(%d,%d,%d,%d)\n", pixel.r, pixel.g, pixel.b, pixel.a);
#endif
				nof_index0 = 0;
			}else if(op == QOI_OP_RGB){
				if(fread(buf, 1, 3, q->f) != 3) return 1;
				pixel.r = buf[0];
				pixel.g = buf[1];
				pixel.b = buf[2];
#ifdef DEBUG
				fprintf(stderr, "QOI_OP_RGBA(%d,%d,%d,%d)\n", pixel.r, pixel.g, pixel.b, pixel.a);
#endif
				nof_index0 = 0;
			}else if((op & QOI_MASK_2) == QOI_OP_INDEX){
				uint8_t index = op;
				/* end sentinel detection */
				if(index == 0 && nof_index0 < 7){
					nof_index0++;
					if(nof_index0>1){
						continue;
					}
#ifdef DEBUG
					fprintf(stderr,"END_MARKER? QOI_OP_INDEX(0) number %d\n", nof_index0);
#endif
				}else if(index == 1 && nof_index0 == 7){
#ifdef DEBUG
					fprintf(stderr,"END_MARKER? QOI_OP_INDEX(1) and %d QOI_OP_INDEX(0)\n", nof_index0);
#endif
					goto END;
				}else{
					nof_index0 = 0;
				}
				pixel = q->index[index];
#ifdef DEBUG
				fprintf(stderr, "QOI_OP_INDEX(%d)\n", index);
#endif
			}else if((op & QOI_MASK_2) == QOI_OP_DIFF){
				int8_t diff_r = ((op >> 4) & 0x03) - 2;
				int8_t diff_g = ((op >> 2) & 0x03) - 2;
				int8_t diff_b = ((op     ) & 0x03) - 2;
				pixel.r += diff_r;
				pixel.g += diff_g;
				pixel.b += diff_b;
#ifdef DEBUG
				fprintf(stderr, "QOI_OP_DIFF(%d,%d,%d)\n", diff_r, diff_g, diff_b);
				fprintf(stderr, "= PIXEL(%d,%d,%d,%d)\n", pixel.r, pixel.g, pixel.b, pixel.a);
#endif
				nof_index0 = 0;
			}else if((op & QOI_MASK_2) == QOI_OP_LUMA){
				if(fread(buf, 1, 1, q->f) != 1) return 1;
				int8_t diff_g = (op & 0x3f) - 32;
				int8_t diff_r_g = (buf[0] >> 4) & 0x0f;
				int8_t diff_b_g = (buf[0]     ) & 0x0f;
				int8_t diff_r = diff_g - 8 + diff_r_g;
				int8_t diff_b = diff_g - 8 + diff_b_g;
				pixel.r += diff_r;
				pixel.g += diff_g;
				pixel.b += diff_b;
#ifdef DEBUG
				fprintf(stderr, "QOI_OP_LUMA(%d,%d,%d)\n", diff_g, diff_r_g, diff_b_g);
				fprintf(stderr, "= QOI_OP_DIFF(%d,%d,%d)\n", diff_r, diff_g, diff_b);
				fprintf(stderr, "= PIXEL(%d,%d,%d,%d)\n", pixel.r, pixel.g, pixel.b, pixel.a);
#endif
				nof_index0 = 0;
			}else if((op & QOI_MASK_2) == QOI_OP_RUN){
				run = ( op & 0x3f ) + 1;
#ifdef DEBUG
				fprintf(stderr, "QOI_OP_RUN(%d)\n", run);
#endif
				nof_index0 = 0;
				continue;
			}else{
#ifdef DEBUG
				fprintf(stderr, "Unknown QOI tag: %0x\n", op);
#endif
				return 1;
			}
			uint8_t index = color_hash(pixel.r, pixel.g, pixel.b, pixel.a);
#ifdef DEBUG
			fprintf( stderr, "STORE_INDEX(%d) = RGBA(%d,%d,%d,%d)\n", index, pixel.r, pixel.g, pixel.b, pixel.a);
#endif
			q->index[index] = pixel;
		}

		assert(ptr<3*img->bufwidth*img->bufheight);
		if(q->header.channels == QOI_CHANNELS_RGB){
			img->buf[ptr++] = pixel.r;
			img->buf[ptr++] = pixel.g;
			img->buf[ptr++] = pixel.b;
		} else if(q->header.channels == QOI_CHANNELS_RGBA){
			float alpha = (float)pixel.a / 255;
			img->buf[ptr++] = ( 1 - alpha ) * background.r + alpha * pixel.r;
			img->buf[ptr++] = ( 1 - alpha ) * background.g + alpha * pixel.g;
			img->buf[ptr++] = ( 1 - alpha ) * background.b + alpha * pixel.b;
		}
	}

END:
#ifdef DEBUG
	fprintf(stderr,"THE END\n");
#endif
	img->state |= LOADED | SLOWLOADED;

	return 0;
}

void qoi_close(struct image *img){
#ifdef DEBUG
	printf("qui_close start\n");
#endif
	struct qoi_t *q = (struct qoi_t *)img;
	fclose(q->f);
}

struct imageformat qoi = {
	qoi_open,
	NULL,
	qoi_read,
	qoi_close
};
