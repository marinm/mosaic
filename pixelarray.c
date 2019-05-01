#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <setjmp.h>

#include "pixelarray.h"
#include <jpeglib.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize.h"

char jpeg_last_error_msg[JMSG_LENGTH_MAX]; 

// Managed memory points
void *mmemp[256];
int n_memp = 0;

void *mmalloc(size_t n) {
	void *p = NULL;
	p = malloc(n);
	assert(p != NULL);
	memset(p, 0, n);
	mmemp[n_memp++] = p;
	return p;
}

void mfreeall() {
	for (int i = 0; i < n_memp; i++)
		free(mmemp[i]);
}

void mfreelast() {
	free(mmemp[--n_memp]);
}


int lint(int a, int b) {
	return (a < b)? a : b;
}

int gint(int a, int b) {
	return (a > b)? a : b;
}

void copy3n(void *dst, void *src, int n) {
	memcpy(dst, src, 3 * n);
}

unsigned char *row(pixelarray *pxarr, int i) {
	return pxarr->RGB + (i * (pxarr->w) * 3);
}

unsigned char *pixel(pixelarray *pxarr, int i, int j) {
	return row(pxarr, i) + (j * 3);
}

void rowcopy(pixelarray *dst, pixelarray *src, int dsti, int dstj, int srci, int srcj, int n) {
	for (int j = 0; j < n; j++)
		copy3n(pixel(dst, dsti, dstj + j),
		       pixel(src, srci, srcj + j), n);
}


void rowfill(pixelarray *pxarr, int i, int r, int g, int b) {
	unsigned char *px = pixel(pxarr, i, 0);
	for (int j = 0; j < pxarr->w; j++) {
		*(px++) = r;
		*(px++) = g;
		*(px++) = b;
	}
}

void colfill(pixelarray *pxarr, int j, int r, int g, int b) {
	unsigned char *px = NULL;
	for (int i = 0; i < pxarr->w; i++) {
		px = pixel(pxarr, i, j);
		*(px++) = r;
		*(px++) = g;
		*(px++) = b;
	}
}

void pixfill(pixelarray *p, int i, int j, int r, int g, int b) {
	unsigned char *px = pixel(p, i, j);
	*(px++) = r;
	*(px++) = g;
	*(px++) = b;
}



void pngcallback(void *context, void *data, int size) {
	// Assume the memory is allocated and big enough
	pixelarray *p = (pixelarray *) context;

	memcpy(p->file + p->flen, data, size);
	p->flen += size;
}
 
int pixelarray_png(pixelarray *p) {
	int r = stbi_write_png_to_func(pngcallback, p, p->w, p->h, p->c, p->RGB, 0);
	return (r == 0)? 0 : 1;
}

void repeat(unsigned char *dst, unsigned char *src, int n, int u, int k, int q) {
	for (int i = 0; i < (n * u * k * q); i++)
		dst[i] = src[(((i / (u * k)) % n) * u) + (i % u)];
}


void quantize(pixelarray *p, int q) {
	for (int i = 0; i < p->h * p->w * 3; i++)
		p->RGB[i] = (p->RGB[i] / q) * q;
}

// A pixel is grout if it is on the edge of the image, or
// its row is a a multiple of k+1 pixels from the edge, or
// its column is a multiple of k+1 pixels from the edge
int isgrout(pixelarray *p, int i, int j, int k) {
	return (i == 0 || i == (p->h - 1) ||
	        j == 0 || j == (p->w - 1) ||
	        (((i % k) == 0) && (i+k <= p->h)) ||
	        (((j % k) == 0) && (j+k <= p->w))
	);	
}









struct jpeg_error_context {
    struct jpeg_error_mgr  errmgr;
    jmp_buf                catch;
};

void jpeg_error_callback(j_common_ptr cinfo)
{
	// https://stackoverflow.com/questions/19857766/error-handling-in-libjpeg

	struct jpeg_error_context *context =
		(struct jpeg_error_context *) cinfo->err;

	//(*cinfo->err->output_message)(cinfo);

	// Put the error message in a buffer
	// (instead of printing to stdout)
	(*(cinfo->err->format_message))(cinfo, jpeg_last_error_msg);
    longjmp(context->catch, 1);
}

int readjpeg(pixelarray *px, unsigned char *fdata, int fdlen) {

	struct jpeg_decompress_struct cinfo;
	struct jpeg_error_context econtext;

	// Change the error handling
	cinfo.err = jpeg_std_error(&econtext.errmgr);
	econtext.errmgr.error_exit = jpeg_error_callback;

	// Establish the setjmp return context for my_error_exit to use
	if (setjmp(econtext.catch)) {
	    // If we get here, the JPEG code has signaled an error
	    jpeg_destroy_decompress(&cinfo);
	    return 0;
	}

	jpeg_create_decompress(&cinfo);

	jpeg_mem_src(&cinfo, fdata, fdlen);

	// Is this a JPEG file?
	if (jpeg_read_header(&cinfo, TRUE) != 1)
		return 0;

	jpeg_start_decompress(&cinfo);
	
	px->w = cinfo.output_width;
	px->h = cinfo.output_height;
	px->c = cinfo.output_components;

	px->RGB = (unsigned char*) malloc(px->w * px->h * px->c);

	while (cinfo.output_scanline < cinfo.output_height) {
		unsigned char *buffer_array[1];
		buffer_array[0] = px->RGB + (cinfo.output_scanline) * (px->w * px->c);

		jpeg_read_scanlines(&cinfo, buffer_array, 1);
	}

	jpeg_finish_decompress(&cinfo);
	jpeg_destroy_decompress(&cinfo);

	return 1;
}


void delpixelarray(pixelarray *p) {
	if (p != NULL) {
		if (p->RGB  != NULL) free(p->RGB);
		if (p->file != NULL) free(p->file);
	}
}





pixelarray *newpixelarray(int h, int w, int c) {
	pixelarray *pxarr = mmalloc(sizeof(pixelarray));

	pxarr->h = h;
	pxarr->w = w;
	pxarr->c = c;
	pxarr->RGB  = malloc(w * h * c);
	return pxarr;
}

pixelarray *squarecrop(pixelarray *pxarr) {
	// The smaller of the dimensions
	// Shave off the remainder rows/pixels to keep multiple of 8
	int s = lint(pxarr->h, pxarr->w);
	s = s - (s % 8);
	pixelarray *square = newpixelarray(s, s, pxarr->c);
	for (int i = 0; i < s; i++)
		rowcopy(square, pxarr, i, 0, i, 0, s);
	return square;
}

pixelarray *squarefit(pixelarray *pxarr, int s) {
	// Put a pixelarray into another one, scaling the longer dimension to s
	pixelarray *fitted = fitdimension(pxarr, s);
	pixelarray *square = newpixelarray(s, s, pxarr->c);

	// Padding colour
	int r = 0xBB;
	int g = 0xBB;
	int b = 0xBB;

	// Center the image horizontally and vertically
	int hslack = s - fitted->w;
	int vslack = s - fitted->h;
	int padding_t = vslack / 2;
	int padding_r = hslack / 2;
	int padding_b = vslack - padding_t;
	int padding_l = hslack - padding_r;

	// Copy the image
	for (int i = 0; i < fitted->h; i++)
		rowcopy(square, fitted, padding_t + i, padding_l, i, 0, fitted->w);

	// Fill in the padding
	for (int i = 0; i < padding_t; i++)
		rowfill(square, i, r, g, b);
	for (int i = 0; i < padding_b; i++)
		rowfill(square, padding_t + fitted->h + i, r, g, b);

	for (int j = 0; j < padding_l; j++)
		colfill(square, j, r, g, b);
	for (int j = 0; j < padding_r; j++)
		colfill(square, padding_l + fitted->w + j, r, g, b);
	
	return square;
}

// Resize a square image to a smaller one
pixelarray *scale(pixelarray *pxarr, int rows, int cols) {
	pixelarray *scaled = newpixelarray(rows, cols, pxarr->c);
    int r = stbir_resize_uint8(pxarr->RGB, pxarr->w, pxarr->h , 0,
            scaled->RGB, cols, rows, 0, pxarr->c);
	if (r == 0) {
		delpixelarray(scaled);
		return 0;
	}
	return scaled;
}

// Make both rows and columns fit some multiple
pixelarray *evenfit(pixelarray *pxarr, int m) {
	int rows = pxarr->h - (pxarr->h % m);
	int cols = pxarr->w - (pxarr->w % m);
	return scale(pxarr, rows, cols);
}

pixelarray *fitdimension(pixelarray *pxarr, int s) {
	// Aspect ratio
	double ratio = (double) gint(pxarr->h, pxarr->w) / (double) s;

	int newrows = (int) ((double) pxarr->h / (double) ratio);
	int newcols = (int) ((double) pxarr->w / (double) ratio);

	// Sometimes the scaling leaves a few extra pixels
	newrows = lint(newrows, s);
	newcols = lint(newcols, s);

	return scale(pxarr, newrows, newcols);
}

// 1 pixel becomes k*k pixels
pixelarray *expand(pixelarray *P, int k) {
	pixelarray *E = newpixelarray(P->h * k, P->w * k, P->c);
	for (int i = 0; i < P->h; i++) 
		repeat(row(E, i * k), row(P, i), P->w, 3, k, k);
	return E;
}

pixelarray *copypixelarray(pixelarray *p) {
	pixelarray *copy = newpixelarray(p->h, p->w, p->c);
	memcpy(copy->RGB, p->RGB, p->h * p->w * 3);
	return copy;
}

// Replace some lines with white
pixelarray *grout(pixelarray *p, int k) {
	pixelarray *G = copypixelarray(p);

	for (int i = 0; i < G->h; i++) {
		for (int j = 0; j < G->w; j++) {
			if (isgrout(G, i, j, k))
				pixfill(G, i, j, 0x00, 0x00, 0x00);
		}
	}
	return G;
}

pixelarray *tiles(pixelarray *p, int fit, int size) {
	pixelarray *E = evenfit(p, size);
	//pixelarray *L = squarefit(E, fit);
	pixelarray *F = fitdimension(E, fit);
	pixelarray *T = expand(F, size);
	pixelarray *G = grout(T, size);

	delpixelarray(E);
	//delpixelarray(L);
	delpixelarray(F);
	delpixelarray(T);
	return G;
}

pixelarray *pixread(unsigned char *file, int len) {

	pixelarray *px = malloc(sizeof(pixelarray));
	memset(px, 0, sizeof px);

	// First try stb
	px->RGB = stbi_load_from_memory(file, len, &(px->w), &(px->h), &(px->c), 3);

	// If that didn't work, try libjpeg
	if (px->RGB == NULL) {
		if (readjpeg(px, file, len) != 1) {
			free(px);
			return NULL;
		}
	}

	return px;
}
