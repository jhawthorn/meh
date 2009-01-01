
#include <stdint.h>
#include <sys/time.h>
#include "meh.h"

#define TDEBUG 0

#define GETVAL0(x, c) ((ibuf[bufxs[x] * 3 + (c)] * (ur) + ibuf[bufxns[x] * 3 + (c)] * (u)) * (vr) >> 20)
#define GETVAL(x, c) (( \
			( \
				ibuf[bufxs[x] * 3 + (c)] * (ur) + \
				ibuf[bufxns[x] * 3 + (c)] * (u) \
			) * (vr) + \
			( \
				(ibufn[bufxs[x] * 3 + (c)]) * (ur) + \
				(ibufn[bufxns[x] * 3 + (c)]) * (u)\
			) * (v)) >> 20)


#if TDEBUG
struct timeval t0;
struct timeval t1;
#endif

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

	unsigned int bufxs[ximg->width];
	unsigned int bufxns[ximg->width];
	unsigned int us[ximg->width];
	unsigned int urs[ximg->width];
	{
		unsigned int dx = (img->bufwidth << 10) / ximg->width;
		unsigned int bufx = img->bufwidth / ximg->width;
		for(x = 0; x < ximg->width; x++){
			if((bufx >> 10) >= img->bufwidth - 1){
				bufxs[x] = img->bufwidth - 1;
				bufxns[x] = 0;
				urs[x] = 1023 ^ (bufx & 1023);
				us[x] = 0;
			}else{
				bufxs[x] = bufx >> 10;
				bufxns[x] = bufxs[x] + 1;
				us[x] = (bufx & 1023);
				urs[x] = 1023 ^ us[x];
			}
			bufx += dx;
		}
	}
	y = 0;
	ibuf = img->buf;
	ibufn = img->buf + dy;
	for(; y < ximg->height; y++){
		unsigned int bufy = (y << 10) * img->bufheight / ximg->height;
		unsigned int v = (bufy & 1023);
		unsigned int vr = 1023^(bufy & 1023);
		ibuf = &img->buf[y * img->bufheight / ximg->height * img->bufwidth * 3];
		ibufn = ibuf + dy;
		if(ibufn > bufend){
			break;
		}else if(ibufn + dy > bufend){
			for(x = 0; x < ximg->width; x++){
				//unsigned int bufx = bufxs[x];
				unsigned int u = us[x];
				unsigned int ur = urs[x];

				*newBuf++ = GETVAL0(x, 2);
				*newBuf++ = GETVAL0(x, 1);
				*newBuf++ = GETVAL0(x, 0);
				newBuf++;
			}
		}else{
			for(x = 0; x < ximg->width; x++){
				//unsigned int bufx = bufxs[x];
				unsigned int u = us[x];
				unsigned int ur = urs[x];

				*newBuf++ = GETVAL(x, 2);
				*newBuf++ = GETVAL(x, 1);
				*newBuf++ = GETVAL(x, 0);
				newBuf++;
			}
		}
		newBuf += jdy;
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


