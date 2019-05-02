// mosaic.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>

#include "frozen.h"
#include "pixelarray.h"

// Limit the size of the client's request text
#define POST_LIMIT 5000000

// Limit the size of the output PNG file
#define PNG_LIMIT 1000000


#define FAILIF(C)                               \
if (C) {                                        \
	request.elines[request.nerr++] = __LINE__;  \
	return 0;                                   \
}

struct {
	char *post;        // The client's complete POST request text
	int   postlen;     // POST payload length

	char *response;    // The complete JSON response 
	int   responselen; // Response length

	char *file;
	int   len;
	int   binlen;

	int   elines[100]; // Error lines
	int   nerr;        // Number of errors
	int   returncode;

	// Temporary variables for testing
	pixelarray px;

	pixelarray *tiles128;
	pixelarray *tiles64;
	pixelarray *tiles32;
	pixelarray *tiles16;
	pixelarray *tiles8;
} request;

int loadrequest() {
	// Zero out the request context
	memset(&request, 0, sizeof request);

	request.post = malloc(POST_LIMIT);
	request.postlen = 0;

	// Copy entire POST payload to memory from stdin
	request.postlen = fread(request.post, 1, POST_LIMIT, stdin);
	FAILIF(request.postlen == 0);

	// Make sure the input is null-terminated
	request.post[request.postlen] = '\0';

	// Parse JSON
 	json_scanf(request.post, request.postlen,
		"{length:%d, file:%V}",
    	&(request.len), &(request.file), &(request.binlen));

	// The POST text is not needed anymore
	free(request.post);
	request.post = NULL;
	return 1;
}

int loadimage() {
	pixelarray *pxarr = pixread(request.file, request.binlen);
	FAILIF(pxarr == NULL)
	request.px = *(pxarr);
	// Do not delpixelarray pxarr, need the contents
	free(pxarr);
	return 1;
}

int transform() {
	// Create a new image with the tiles effect
	request.tiles128 = tiles(&request.px, 128, 4);
	FAILIF(request.tiles128 == NULL);
	// Convert the pixelarray to a PNG stream
	request.tiles128->file = malloc(PNG_LIMIT);
	request.tiles128->flen = 0;
	request.tiles128->flim = PNG_LIMIT;
	FAILIF(pixelarray_png(request.tiles128) == 0);

	// Create a new image with the tiles effect
	request.tiles64 = tiles(&request.px, 64, 8);
	FAILIF(request.tiles64 == NULL);
	// Convert the pixelarray to a PNG stream
	request.tiles64->file = malloc(PNG_LIMIT);
	request.tiles64->flen = 0;
	request.tiles64->flim = PNG_LIMIT;
	FAILIF(pixelarray_png(request.tiles64) == 0);

	// Create a new image with the tiles effect
	request.tiles32 = tiles(&request.px, 32, 16);
	FAILIF(request.tiles32 == NULL);
	// Convert the pixelarray to a PNG stream
	request.tiles32->file = malloc(PNG_LIMIT);
	request.tiles32->flen = 0;
	request.tiles32->flim = PNG_LIMIT;
	FAILIF(pixelarray_png(request.tiles32) == 0);

	// Create a new image with the tiles effect
	request.tiles16 = tiles(&request.px, 16, 32);
	FAILIF(request.tiles16 == NULL);
	// Convert the pixelarray to a PNG stream
	request.tiles16->file = malloc(PNG_LIMIT);
	request.tiles16->flen = 0;
	request.tiles16->flim = PNG_LIMIT;
	FAILIF(pixelarray_png(request.tiles16) == 0);

	// Create a new image with the tiles effect
	request.tiles8 = tiles(&request.px, 8, 64);
	FAILIF(request.tiles8 == NULL);
	// Convert the pixelarray to a PNG stream
	request.tiles8->file = malloc(PNG_LIMIT);
	request.tiles8->flen = 0;
	request.tiles8->flim = PNG_LIMIT;
	FAILIF(pixelarray_png(request.tiles8) == 0);

	return 1;
}

int printjson() {
	// Write the response to stdout
	printf("Content-Type: application/json; charset=utf-8;\r\n\r\n");

	struct json_out out = JSON_OUT_FILE(stdout);
	request.responselen = json_printf(&out,
		"{elines:%d, tiles128:%V, tiles64:%V, tiles32:%V, tiles16:%V, tiles8:%V}\r\n",
		request.elines[0],
		request.tiles128->file, request.tiles128->flen,
		request.tiles64->file , request.tiles64->flen,
		request.tiles32->file , request.tiles32->flen,
		request.tiles16->file , request.tiles16->flen,
		request.tiles8->file  , request.tiles8->flen
	);
	return 1;
}

void cleanup() {
	delpixelarray(&request.px);
	//delpixelarray(request.tf);
}

int main() {
	// Copy the POST payload to memory and parse JSON contents
	loadrequest();

	// Serealize the image to an RGB sequence
	loadimage();

	// Image transform
	transform();

	// Print the results
	printjson();

	// Free memory
	cleanup();

	return 0;
}
