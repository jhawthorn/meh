
#include <stdio.h>

struct image;

struct imageformat{
	struct image *(*open)(FILE *);
	int (*read)(struct image *);
};

struct image{
	unsigned char *buf;
	int width, height;
	FILE *f;
	struct imageformat *fmt;
};

unsigned char *loadgif(FILE *, int *, int *);
unsigned char *loadjpeg(FILE *, int *, int *);
unsigned char *loadpng(FILE *, int *, int *);

