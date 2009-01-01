
#include <stdint.h>
#include <sys/time.h>
#include "meh.h"

#define TDEBUG 0

#define GETVAL0(c) ((ibuf[x0 + (c)] * (ur) + ibuf[x1 + (c)] * (u)) * (vr) >> 20)
#define GETVAL(c) (( \
			( \
				ibuf[x0 + (c)] * (ur) + \
				ibuf[x1 + (c)] * (u) \
			) * (vr) + \
			( \
				(ibufn[x0 + (c)]) * (ur) + \
				(ibufn[x1 + (c)]) * (u)\
			) * (v)) >> 20)


#if TDEBUG
struct timeval t0;
struct timeval t1;
#endif

#define XLOOP(F) \
	for(x = 0; x < ximg->width*4;){ \
		const unsigned int x0 = a[x++];\
		const unsigned int x1 = a[x++];\
		const unsigned int u  = a[x++];\
		const unsigned int ur = a[x++];\
		*newBuf++ = F(2);\
		*newBuf++ = F(1);\
		*newBuf++ = F(0);\
		newBuf++;\
	}

#define YITER \
	const unsigned int bufy = (y << 10) * img->bufheight / ximg->height;\
	const unsigned int v = (bufy & 1023);\
	const unsigned int vr = 1023^(bufy & 1023);\
	ibuf = &img->buf[y * img->bufheight / ximg->height * img->bufwidth * 3];\
	ibufn = ibuf + dy;


void scale(struct image *img, XImage *ximg){
	int x, y;
	const unsigned char * __restrict__ ibuf;
	const unsigned char * __restrict__ ibufn;
	const unsigned char * const bufend = &img->buf[img->bufwidth * img->bufheight * 3];
	char* __restrict__ newBuf = ximg->data;
	const unsigned int jdy = ximg->bytes_per_line / 4 - ximg->width;
	const unsigned int dy = img->bufwidth * 3;

#if TDEBUG
	gettimeofday(&t0, NULL);
#endif

	unsigned int a[ximg->width * 4];
	{
		unsigned int dx = (img->bufwidth << 10) / ximg->width;
		unsigned int bufx = img->bufwidth / ximg->width;
		for(x = 0; x < ximg->width * 4;){
			if((bufx >> 10) >= img->bufwidth - 1){
				a[x++] = (img->bufwidth - 1) * 3;
				a[x++] = (img->bufwidth - 1) * 3;
				a[x++] = 0;
				a[x++] = 1023 ^ (bufx & 1023);
			}else{
				a[x++] = (bufx >> 10) * 3;
				a[x++] = ((bufx >> 10) + 1) * 3;
				a[x++] = (bufx & 1023);
				a[x++] = 1023 ^ (bufx & 1023);
			}
			bufx += dx;
		}
	}
	y = 0;
	ibuf = img->buf;
	ibufn = img->buf + dy;
	for(;;){
		YITER
		if(ibufn + dy > bufend){
			break;
		}
		XLOOP(GETVAL)
		newBuf += jdy;
		y++;
	}
	for(;;){
		YITER
		if(ibufn > bufend){
			break;
		}
		XLOOP(GETVAL0)
		newBuf += jdy;
		y++;
	}

#if TDEBUG
	gettimeofday(&t1, NULL);
	printf("%li x100us\n", ((t1.tv_sec - t0.tv_sec) * 1000000 + t1.tv_usec - t0.tv_usec) / 100);
#endif
}

void linearscale(struct image *img, XImage *ximg){
	int x, y;
	unsigned char * __restrict__ ibuf;
	char* __restrict__ newBuf = ximg->data;
	unsigned int jdy = ximg->bytes_per_line / 4 - ximg->width;
	unsigned int dx = (img->bufwidth << 10) / ximg->width;

#if TDEBUG
	gettimeofday(&t0, NULL);
#endif

	for(y = 0; y < ximg->height; y++){
		unsigned int bufx = img->bufwidth / ximg->width;
		ibuf = &img->buf[y * img->bufheight / ximg->height * img->bufwidth * 3];

		for(x = 0; x < ximg->width; x++){
			*newBuf++ = (ibuf[(bufx >> 10)*3+2]);
			*newBuf++ = (ibuf[(bufx >> 10)*3+1]);
			*newBuf++ = (ibuf[(bufx >> 10)*3+0]);
			newBuf++;
			bufx += dx;
		}
		newBuf += jdy;
	}

#if TDEBUG
	gettimeofday(&t1, NULL);
	printf("%li x100us\n", ((t1.tv_sec - t0.tv_sec) * 1000000 + t1.tv_usec - t0.tv_usec) / 100);
#endif
}


