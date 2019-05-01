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
	pixelarray *tf;
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
	request.tf = tiles(&request.px, 32, 16);
	FAILIF(request.tf == NULL);

	// Convert the pixelarray to a PNG stream
	request.tf->file = malloc(PNG_LIMIT);
	request.tf->flen = 0;
	request.tf->flim = PNG_LIMIT;
	FAILIF(pixelarray_png(request.tf) == 0);

	return 1;
}

int printjson() {
	// Write the response to stdout
	printf("Content-Type: application/json; charset=utf-8;\r\n\r\n");

	struct json_out out = JSON_OUT_FILE(stdout);
	request.responselen = json_printf(&out,
		"{elines:%d, transform:%V, w:%d, h:%d}\r\n",
		request.elines[0],
		request.tf->file, request.tf->flen,
		request.tf->w, request.tf->h);
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
