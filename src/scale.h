#ifndef SCALE_H
#define SCALE_H SCALE_H

/* scale */
void scale(struct image *img, unsigned int width, unsigned int height, unsigned int bytesperline, char* __restrict__ newBuf);
void nearestscale(struct image *img, unsigned int width, unsigned int height, unsigned int bytesperline, char* __restrict__ newBuf);

#endif

