
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

