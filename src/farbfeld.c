#include "meh.h"

#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <assert.h>

/* #define DEBUG */

struct farbfeld_header_t {
	unsigned char magic[8]; /* magic bytes "farbfeld" */
	uint32_t width; /* image width in pixels (BE) */
	uint32_t height; /* image height in pixels (BE) */
};

#define FARBFELD_MAGIC_LEN 8
static unsigned char FARBFELD_MAGIC[FARBFELD_MAGIC_LEN] = "farbfeld";

struct farbfeld_rgba_t {
	uint16_t r, g, b, a;
};

struct farbfeld_t{
	struct image img;
	FILE *f;
	struct farbfeld_header_t header;
};

struct image *farbfeld_open(FILE *f){
	struct farbfeld_t *q;
	uint32_t buf;

#ifdef DEBUG
	printf("farbfeld_open start\n");
#endif

	rewind(f);
	q = malloc(sizeof(struct farbfeld_t));
	if(!q) return NULL;
	q->f = f;

	/* read header */
	if(fread(q->header.magic, 1, FARBFELD_MAGIC_LEN, f) != FARBFELD_MAGIC_LEN)
		return NULL;
	if(memcmp(q->header.magic, FARBFELD_MAGIC, FARBFELD_MAGIC_LEN) != 0)
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

	q->img.bufwidth = q->header.width;
	/* we add one row to the output buffer, so we can put the sentinel there,
	 * below we make sure we don't write pixels after we past the last row.
	 */
	q->img.bufheight = q->header.height+1;
	
	q->img.fmt = &farbfeld;

	return (struct image *)q;
}

int farbfeld_read(struct image *img){
	struct farbfeld_t *q = (struct farbfeld_t *)img;
	uint16_t buf[4];
	uint32_t ptr = 0;
	struct farbfeld_rgba_t pixel;
	uint32_t rows, cols;

#ifdef DEBUG
	printf("farbfeld_read start\n");
#endif

	rows = 0;
	cols = 0;

	while(1){
		if(fread(buf, 1, 8, q->f) != 1) {
			if(feof(q->f)){
				fprintf(stderr, "EOF seen, premature end of image? %d, %d\n", rows, cols);
				goto END;
			}
		}
		pixel.r = ntohs(buf[0]);
		pixel.g = ntohs(buf[1]);
		pixel.b = ntohs(buf[2]);
		pixel.a = ntohs(buf[3]);

#ifdef DEBUG
		fprintf(stderr, "PIXEL(%d,%d) = RGBA(%d,%d,%d,%d)\n", rows, cols, pixel.r, pixel.g, pixel.b, pixel.a);
#endif
		float alpha = pixel.a / 65535;
		img->buf[ptr++] = ( alpha * pixel.r ) / 257;
		img->buf[ptr++] = ( alpha * pixel.g ) / 257;
		img->buf[ptr++] = ( alpha * pixel.b ) / 257;
#ifdef DEBUG
		fprintf(stderr, "BUF(%d,%d)=(%d,%d,%d)\n", rows, cols, img->buf[ptr-3], img->buf[ptr-2], img->buf[ptr-1]);
#endif
		
		cols++;
		if(cols >= q->header.width){
			cols=0;
			rows++;
			if(rows >= q->header.height){
				goto END;
			}
		}
	}

END:
#ifdef DEBUG
	fprintf(stderr,"THE END\n");
#endif
	img->state |= LOADED | SLOWLOADED;
	
	return 0;
}

void farbfeld_close(struct image *img){
	struct farbfeld_t *q = (struct farbfeld_t *)img;

#ifdef DEBUG
	printf("farbfeld_close start\n");
#endif
	fclose(q->f);
}

struct imageformat farbfeld = {
	farbfeld_open,
	NULL,
	farbfeld_read,
	farbfeld_close
};
