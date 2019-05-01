// mosaic.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>

#include "frozen.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <jpeglib.h>

typedef struct {
	unsigned char * RGB   ;  // RGB representation
	int             w     ;  // Width
	int             h     ;  // Height
	int             c     ;  // Pixel components
} pixelmap;

struct {
	char * file;
	int    len;    // file len
	int    b64len;
	char * response;
	int    response_len;
	int    returncode;

	// Temporary variables for testing
	pixelmap px;
} request;

struct jpeg_error_context {
    struct jpeg_error_mgr  pub;
    jmp_buf                setjmp_buffer;
};

char jpeg_last_error_msg[JMSG_LENGTH_MAX];

void jpegErrorExit(j_common_ptr cinfo)
{
	// https://stackoverflow.com/questions/19857766/error-handling-in-libjpeg

    // output_message is a method to print an error message
    //(* (cinfo->err->output_message) ) (cinfo);

    // Create the message
    // (*(cinfo->err->format_message))(cinfo, jpegLastErrorMsg);

	struct jpeg_error_context *context =
		(struct jpeg_error_context *) cinfo->err;

    /* Jump to the setjmp point */
    longjmp(context->setjmp_buffer, 1);
}

int readjpeg(pixelmap *px, unsigned char *fdata, int fdlen) {

	struct jpeg_decompress_struct cinfo;
	struct jpeg_error_context jerr;

	// Set the error manager
	cinfo.err = jpeg_std_error(&jerr.pub);

	// Call this function on error
	jerr.pub.error_exit = jpegErrorExit;

	// Establish the setjmp return context for my_error_exit to use
	if (setjmp(jerr.setjmp_buffer)) {
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

int loadrequest() {
	unsigned char *post_payload = malloc(5000000);
	size_t payload_len = 0;

	// Zero out the request context
	memset(&request, 0, sizeof request);

	// Copy POST payload to memory
	// The JSON tokenizer will make a copy of this, too.
	payload_len = fread(post_payload, 1, 5000000, stdin);
	if (payload_len == 0)
		return 0;

	// Make sure the input is null-terminated
	post_payload[payload_len] = '\0';

	// Tokenize JSON
 	json_scanf(post_payload, payload_len, "{length:%d, file:%V}",
    	&(request.len), &(request.file), &(request.b64len));

	if (request.b64len == 0)
		return 0;

	free(post_payload);
	return 1;
}

int loadimage() {
	// First try the stb
	request.px.RGB = stbi_load_from_memory(request.file, request.b64len,
		&(request.px.w), &(request.px.h), &(request.px.c), 3);

	// If that didn't work, try libjpeg
	if (request.px.RGB == NULL) {
		request.returncode++;
		if (readjpeg(&(request.px), request.file, request.len) != 1)
			request.returncode++;
	}
}

int printjson() {
	// Write the response to stdout
	struct json_out jsonout = JSON_OUT_FILE(stdout);
	printf("Content-Type: application/json; charset=utf-8;\r\n\r\n");
	request.response_len = json_printf(&jsonout,
		"{returncode:%d,len:%d,b64len:%d,w:%d,h:%d,c:%d}\r\n",
		request.returncode, request.len, request.b64len, request.px.w,
		request.px.h, request.px.c);
	return 1;
}

void cleanup() {
	free(request.px.RGB);
	free(request.file);
	free(request.response);
}

int main() {
	// Copy the POST payload to memory and parse JSON contents
	loadrequest();

	// Serealize the image to an RGB sequence
	loadimage();

	// Print the results
	printjson();

	// Free memory
	cleanup();

	return 0;
}
