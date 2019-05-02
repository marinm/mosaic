#ifndef PIXELARRAY_H
#define PIXELARRAY_H

typedef struct {
	unsigned char * RGB   ;  // RGB representation
	int             w     ;  // Width
	int             h     ;  // Height
	int             c     ;  // Pixel components

	unsigned char * file  ;  // File (PNG/JPEG) representation
	int             flim  ;  // File size limit
	int             flen  ;  // File length
} pixelarray;


pixelarray *newpixelarray    (int h, int w, int c);
pixelarray *pixread          (unsigned char *file, int len);
int grout            (pixelarray *p, int k);
pixelarray *evenfit          (pixelarray *p, int m);
pixelarray *fitdimension     (pixelarray *p, int s);
pixelarray *squarecrop       (pixelarray *p);
pixelarray *squarefit        (pixelarray *p, int s);
pixelarray *scale            (pixelarray *p, int rows, int cols);
pixelarray *evenfit          (pixelarray *p, int m);
pixelarray *fitdimension     (pixelarray *p, int s);
pixelarray *copypixelarray   (pixelarray *p);
pixelarray *expand           (pixelarray *p, int k);
pixelarray *tiles            (pixelarray *p, int fit, int size);

int         pixelarray_png   (pixelarray *p);
void        delpixelarray    (pixelarray *p);

#endif
