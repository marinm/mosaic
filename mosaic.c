// mosaic.c

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>

#include "frozen.h"
#include "mosaic.h"
#include "colourstring.h"


int setup() {
	// Zero out the request context
	memset(&request, 0, sizeof request);
}


int cleanup() {
	// The JSON parser allocates a big chunk for the base64 conversion
	free(request.userimg);
}


// Copy all of stdin to memory, up to some limit
// This function creates a new memory block.
int copystdin() {
	// Allocate a big block of memory
	request.post = malloc(POST_LIMIT);
	request.postlen = 0;

	// Copy entire POST payload to memory from stdin.
	// This is simplistic but works most of the time.
	// fread does not guarantee that it will read until EOF.
	request.postlen = fread(request.post, 1, POST_LIMIT, stdin);
	assert(request.postlen != 0);

	// Make sure the input is null-terminated
	request.post[request.postlen] = '\0';

	return 1;
}


int loadrequest() {
	// Parse JSON
	// The parser allocates memory for its output
 	json_scanf(request.post, request.postlen,
		"{file:%V}",
    	&(request.userimg), &(request.binlen));

	// The post body is not needed anymore
	free(request.post);

	// Just to be extra safe...
	request.post = NULL;

	return 1;
}

int loadimage() {
	request.ispng = detect_png(request.userimg);
	request.isjpg = detect_jpg(request.userimg);

	request.w = 25;
	request.h = 26;

	return 1;
}

int printjson() {
	// Write the response to stdout
	printf("Content-Type: application/json; charset=utf-8;\r\n\r\n");

	struct json_out out = JSON_OUT_FILE(stdout);
	request.responselen = json_printf(&out,
		"{w:%d, h:%d, ispng:%d, isjpg:%d}\r\n",
		request.w, request.h, request.ispng, request.isjpg
	);

	return 1;
}

