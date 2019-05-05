// mosaic.c
// Reads in an image file and returns a transformed image that looks
// like a mosaic. Input and output are both JSON.
//
// SECTIONS IN THIS FILE
//   * Includes
//   * Global variables
//   * Function declarations
//   * Main
//   * Mosaic
//   * Services
//   * Colourstring
//

// -- INCLUDES --------------------------------------------------------

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>

#include "frozen.h"
#include "jpeglib.h"
#include "png.h"

///////////////////////////////////////////////////////////////////////


// -- GLOBAL VARIABLES --------------------------------------------------------

// Limit the size of the client's request text
#define POST_LIMIT 5000000

// Limit the size of the output PNG file
#define PNG_LIMIT 1000000

// Global/singleton handle
struct {
	char *post;        // The client's complete POST request text
	int   postlen;     // POST payload length

	char *response;    // The complete JSON response 
	int   responselen; // Response length

	char *userimg;
	int   userimglen;

	int size;
	int w;
	int h;
	int c;

	int ispng;
	int isjpg;

	int errno;
} request;


///////////////////////////////////////////////////////////////////////////////


// -- FUNCTIONS DECLARATIONS --------------------------------------------------

// If a function sets the errno flag, future functions should not do anything.
// The hot potato is passed down the stack.
#define HOTPOTATO if (request.errno != 0) {return 0;}

// Requiring makes the potato hot.
// A function should not leave dangling pointers before requiring.
// Note that return values and errno treat 1/0 differently.
#define REQUIRE(C) if (!(C)) {request.errno = __LINE__; return 0;}

int setup();
int doservice();
int printresponse();
int cleanup();

int loaduserjpg();

int detect_png(unsigned char *stream);
int detect_jpg(unsigned char *stream);

int read_JPEG_file(unsigned char *jpg_buffer, unsigned long jpg_size,
	unsigned char *outstream, unsigned long *outlen, int *w, int *h, int *c);

int copystdin();
int loadrequest();

int service_getimgattributes();

///////////////////////////////////////////////////////////////////////////////


// -- MAIN ------------------------------------------------------------

int main() {
	// Prepare memory, load and parse the request
	setup();

	// Handle the request
	doservice();

	// Print the results
	printresponse();

	// Free memory
	cleanup();

	return 0;
}

///////////////////////////////////////////////////////////////////////


// -- MOSAIC ----------------------------------------------------------

int setup() {
	// Zero out the request context
	memset(&request, 0, sizeof request);

	// The errno indicates if future functions should even execute.
	request.errno = 0;

	REQUIRE(loadrequest());

	return 1;
}


// Copy the POST payload to memory and parse the JSON contents
int loadrequest() {

	HOTPOTATO

	// Copy the POST payload to memory
	REQUIRE(copystdin());

	// Parse JSON
	// The parser allocates memory for its output
 	json_scanf(request.post, request.postlen,
		"{file:%V}",
    	&(request.userimg), &(request.userimglen));

	// The post body is not needed anymore
	free(request.post);

	// Just to be extra safe...
	request.post = NULL;

	// An empty request 
	REQUIRE(request.userimglen > 0);
	REQUIRE(request.userimg != NULL);

	return 1;
}



// Copy all of stdin to memory, up to some limit
// This function creates a new memory block.
int copystdin() {

	HOTPOTATO

	// Allocate a big block of memory
	request.post = malloc(POST_LIMIT);

	REQUIRE(request.post != NULL);

	// Copy entire POST payload to memory from stdin.
	// This is simplistic but works most of the time.
	// fread does not guarantee that it will read until EOF.
	request.postlen = 0;
	request.postlen = fread(request.post, 1, POST_LIMIT, stdin);
	assert(request.postlen != 0);

	// Make sure the input is null-terminated
	request.post[request.postlen] = '\0';

	return 1;
}


int doservice() {

	HOTPOTATO

	REQUIRE(service_getimgattributes());

	return 1;
}

int printresponse() {

	// Write the response to stdout
	printf("Content-Type: application/json; charset=utf-8;\r\n\r\n");

	struct json_out out = JSON_OUT_FILE(stdout);
	request.responselen = json_printf(&out,
		"{errno:%d, w:%d, h:%d, ispng:%d, isjpg:%d}\r\n",
		request.errno, request.w, request.h, request.ispng, request.isjpg
	);

	// Still, relay whether the errno is set
	REQUIRE(request.errno == 0);

	return 1;
}


int cleanup() {
	// The JSON parser allocates a big chunk for the base64 conversion
	if (request.userimg != NULL)
		free(request.userimg);
}

///////////////////////////////////////////////////////////////////////


// -- SERVICES --------------------------------------------------------

// Input:
//   - a PNG/JPG file loaded entirely in long array
// Output:
//
int service_getimgattributes() {

	HOTPOTATO

	request.ispng = detect_png(request.userimg);
	request.isjpg = detect_jpg(request.userimg);

	request.w = 0;
	request.h = 0;

	if (request.isjpg)
		REQUIRE(loaduserjpg());

	return 1;
}

int loaduserjpg() {

	HOTPOTATO

	REQUIRE( read_JPEG_file(request.userimg, request.userimglen,
		NULL, NULL, &(request.w), &(request.h), &(request.c)) == 1 );

	return 1;
}


///////////////////////////////////////////////////////////////////////


// -- COLOURSTRING ------------------------------------------------------------

// SECTION PNG

// Check if the stream starts like a PNG file
// 1 means yes, 0 means no.
int detect_png(unsigned char *stream) {

	HOTPOTATO

	return (png_check_sig(stream, 8) == 0)? 0 : 1;
}

// END PNG


// SECTION JPEG

// https://raw.githubusercontent.com/libjpeg-turbo/
//  libjpeg-turbo/master/example.txt

//
// ERROR HANDLING:
//
// The JPEG library's standard error handler (jerror.c) is divided into
// several "methods" which you can override individually.  This lets you
// adjust the behavior without duplicating a lot of code, which you might
// have to update with each future release.
//
// Our example here shows how to override the "error_exit" method so that
// control is returned to the library's caller when a fatal error occurs,
// rather than calling exit() as the standard error_exit method does.
//
// We use C's setjmp/longjmp facility to return control.  This means that the
// routine which calls the JPEG library must first execute a setjmp() call to
// establish the return point.  We want the replacement error_exit to do a
// longjmp().  But we need to make the setjmp buffer accessible to the
// error_exit routine.  To do this, we make a private extension of the
// standard JPEG error handler object.  (If we were using C++, we'd say we
// were making a subclass of the regular error handler.)
//
// Here's the extended error handler struct:
//

struct my_error_mgr {
  struct jpeg_error_mgr pub;  // public fields
  jmp_buf setjmp_buffer;      // for return to caller
};

// Here's the routine that will replace the standard error_exit method:
void
my_error_exit(j_common_ptr cinfo)
{
	// cinfo->err really points to a my_error_mgr struct, so coerce pointer
	struct my_error_mgr *myerr = (struct my_error_mgr *) cinfo->err;
	
	// Always display the message.
	// We could postpone this until after returning, if we chose.
	//(*cinfo->err->output_message) (cinfo);

	// Return control to the setjmp point
	longjmp(myerr->setjmp_buffer, 1);
}


int detect_jpg(unsigned char *stream) {

	HOTPOTATO

	// This struct contains the JPEG decompression parameters and pointers to
	// working space (which is allocated as needed by the JPEG library).
	struct jpeg_decompress_struct cinfo;
	// We use our private extension JPEG error handler.
	// Note that this struct must live as long as the main JPEG parameter
	// struct, to avoid dangling-pointer problems.
	struct my_error_mgr jerr;

	// We set up the normal JPEG error routines, then override error_exit.
	cinfo.err = jpeg_std_error(&jerr.pub);
	jerr.pub.error_exit = my_error_exit;
	// Establish the setjmp return context for my_error_exit to use.
	if (setjmp(jerr.setjmp_buffer)) {
		// If we get here, the JPEG code has signaled an error.
		jpeg_destroy_decompress(&cinfo);
		return 0;
	}
	// Now we can initialize the JPEG decompression object.
	jpeg_create_decompress(&cinfo);
	
	// Step 2: specify data source (eg, a file)

	// Set a fake stream size. Just interested in dimensions
	jpeg_mem_src(&cinfo, stream, 10000);

	(void)jpeg_read_header(&cinfo, TRUE);
	// We can ignore the return value from jpeg_read_header since
	//   (a) suspension is not possible with the stdio data source, and
	//   (b) we passed TRUE to reject a tables-only JPEG file as an error.
	// See libjpeg.txt for more info.
	
	// Step 4: set parameters for decompression
	
	// In this example, we don't need to change any of the defaults set by
	// jpeg_read_header(), so we do nothing here.
	
	// Step 5: Start decompressor
	
	(void)jpeg_start_decompress(&cinfo);
	// We can ignore the return value since suspension is not possible
	// with the stdio data source.

	jpeg_destroy_decompress(&cinfo);

	return 1;
}


// Read a JPEG file and write it as a string of RGB values
int read_JPEG_file(unsigned char *jpg_buffer, unsigned long jpg_size,
	unsigned char *outstream, unsigned long *outlen, int *w, int *h, int *c)
{
	HOTPOTATO

	// This struct contains the JPEG decompression parameters and pointers to
	// working space (which is allocated as needed by the JPEG library).
	struct jpeg_decompress_struct cinfo;
	// We use our private extension JPEG error handler.
	// Note that this struct must live as long as the main JPEG parameter
	// struct, to avoid dangling-pointer problems.
	struct my_error_mgr jerr;
	// More stuff
	JSAMPARRAY buffer;  // Output row buffer
	int row_stride;     // physical row width in output buffer
	
	// Step 1: allocate and initialize JPEG decompression object
	
	// We set up the normal JPEG error routines, then override error_exit.
	cinfo.err = jpeg_std_error(&jerr.pub);
	jerr.pub.error_exit = my_error_exit;
	// Establish the setjmp return context for my_error_exit to use.
	if (setjmp(jerr.setjmp_buffer)) {
		// If we get here, the JPEG code has signaled an error.
		// We need to clean up the JPEG object, close the input file, and return.
		jpeg_destroy_decompress(&cinfo);
		return 0;
	}
	// Now we can initialize the JPEG decompression object.
	jpeg_create_decompress(&cinfo);
	
	// Step 2: specify data source (eg, a file)

	jpeg_mem_src(&cinfo, jpg_buffer, jpg_size);

	// Step 3: read file parameters with jpeg_read_header()

	(void)jpeg_read_header(&cinfo, TRUE);
	// We can ignore the return value from jpeg_read_header since
	//   (a) suspension is not possible with the stdio data source, and
	//   (b) we passed TRUE to reject a tables-only JPEG file as an error.
	// See libjpeg.txt for more info.
	
	// Step 4: set parameters for decompression
	
	// In this example, we don't need to change any of the defaults set by
	// jpeg_read_header(), so we do nothing here.
	
	// Step 5: Start decompressor
	
	(void)jpeg_start_decompress(&cinfo);
	// We can ignore the return value since suspension is not possible
	// with the stdio data source.

	// Set the dimensions and colour components 
	*w = cinfo.output_width;
	*h = cinfo.output_height;
	*c = cinfo.output_components;
	
	// We may need to do some setup of our own at this point before reading
	// the data.  After jpeg_start_decompress() we have the correct scaled
	// output image dimensions available, as well as the output colormap
	// if we asked for color quantization.
	// In this example, we need to make an output work buffer of the right size.

	// JSAMPLEs per row in output buffer
	//row_stride = cinfo.output_width * cinfo.output_components;

	// Make a one-row-high sample array that will go away when done with image
	//buffer = (*cinfo.mem->alloc_sarray)
	//              ((j_common_ptr)&cinfo, JPOOL_IMAGE, row_stride, 1);
	
	// Step 6: while (scan lines remain to be read) */
	//           jpeg_read_scanlines(...); */
	
	// Here we use the library's state variable cinfo.output_scanline as the
	// loop counter, so that we don't have to keep track ourselves.
	//int next = 0;
	//while (cinfo.output_scanline < cinfo.output_height) {
	//	// jpeg_read_scanlines expects an array of pointers to scanlines.
	//	// Here the array is only one element long, but you could ask for
	//	// more than one scanline at a time if that's more convenient.
	//	(void)jpeg_read_scanlines(&cinfo, buffer, 1);

	//	// Copy the data to the output stream
	//	memcpy(outstream + next, buffer[0], row_stride);
	//	next += row_stride;
	//}
	//*outlen = next;
	
	// Step 7: Finish decompression
	
	//(void)jpeg_finish_decompress(&cinfo);
	// We can ignore the return value since suspension is not possible
	// with the stdio data source.
	
	// Step 8: Release JPEG decompression object
	
	// This is an important step since it will release a good deal of memory.
	jpeg_destroy_decompress(&cinfo);
	
	// At this point you may want to check to see whether any corrupt-data
	// warnings occurred (test whether jerr.pub.num_warnings is nonzero).
	
	// And we're done!
	return 1;
}


//
// SOME FINE POINTS:
//
// In the above code, we ignored the return value of jpeg_read_scanlines,
// which is the number of scanlines actually read.  We could get away with
// this because we asked for only one line at a time and we weren't using
// a suspending data source.  See libjpeg.txt for more info.
//
// We cheated a bit by calling alloc_sarray() after jpeg_start_decompress();
// we should have done it beforehand to ensure that the space would be
// counted against the JPEG max_memory setting.  In some systems the above
// code would risk an out-of-memory error.  However, in general we don't
// know the output image dimensions before jpeg_start_decompress(), unless we
// call jpeg_calc_output_dimensions().  See libjpeg.txt for more about this.
//
// Scanlines are returned in the same order as they appear in the JPEG file,
// which is standardly top-to-bottom.  If you must emit data bottom-to-top,
// you can use one of the virtual arrays provided by the JPEG memory manager
// to invert the data.  See wrbmp.c for an example.
//
// As with compression, some operating modes may require temporary files.
// On some systems you may need to set up a signal handler to ensure that
// temporary files are deleted if the program is interrupted.  See libjpeg.txt.

// END JPEG

///////////////////////////////////////////////////////////////////////


