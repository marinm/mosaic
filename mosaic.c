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

#include "jpeglib.h"
#include "png.h"
#include "exoquant.h"
#include "frozen.h"

///////////////////////////////////////////////////////////////////////


// -- GLOBAL VARIABLES --------------------------------------------------------

// Limit the size of the client's request text
#define POST_LIMIT 5000000

// Limit the size of the RGB expansion
// The RGB expansion is almost certainly more than 3x larger
// than the input file
#define RGB_LIMIT 30000000

// Maximum image height
#define ROWS_LIMIT 4000

// Limit the size of the output PNG file
#define PNG_LIMIT 1000000

// Limit the capacity of notes (warnings/errors)
#define NOTES_LIMIT 100000

// Number of colours in quantization
#define PALETTE_N 256

// The common properties of various file formats representing
// a rectangular array of colour values (i.e. raster image)
typedef struct {
	// The entire file should be loaded in a large array
	unsigned char  *file;
	unsigned int    filelen;

	unsigned int    width;
	unsigned int    height;
	unsigned int    channels;

	// The entire raster RGB conversion should fit in the
	// allocated array provided (with given limit)
	unsigned char  *rgb;
	unsigned char   rgblen;
	unsigned char   rgblimit;
	unsigned char **rows;
	// rgblen = width*height*components

	// Let any function that operates on this struct log
	// a note using this function. Used for capturing library
	// warnings and errors that would otherwise crash this
	// process.
	void         (*makenote)(const char *note);

	// Bookkeeping states
	unsigned int   readprogress;
} rasterimagefile;


// Global/singleton handle
struct {
	char *post;        // The client's complete POST request text
	int   postlen;     // POST payload length

	char *response;    // The complete JSON response 
	int   responselen; // Response length

	rasterimagefile
	      img;         // The user's provided image, and rgb conversion

	int size;
	int w;
	int h;
	int c;

	int ispng;
	int isjpg;

	unsigned char palette[PALETTE_N * 4];

	int errno;
	char *notes;
} request;



///////////////////////////////////////////////////////////////////////////////


// -- FUNCTIONS DECLARATIONS --------------------------------------------------

// If a function sets the errno flag, future functions should not do anything.
// The hot potato is passed down the stack.
#define HOTPOTATO if (request.errno != 0) {return 0;}

// Requiring makes the potato hot.
// A function should not leave dangling pointers before requiring.
// Note that return values and errno treat 1/0 differently.
#define REQUIRE(C)             \
	if (!(C)) {                \
		request.errno++;       \
		potatostack(__LINE__); \
		return 0;              \
	}

int setup();
int doservice();
int printresponse();
int cleanup();


int detect_png(unsigned char *stream);
int detect_jpg(unsigned char *stream);

int loaduserjpg();
int loaduserpng();

int read_JPG_file(rasterimagefile *image);
int read_PNG_file(rasterimagefile *image);

int copystdin();
int loadrequest();

int service_getimgattributes();
int service_palette();

void potatostack(int linenum);
void makenote(const char *note);

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

	// Zero out the raster image file handle
	memset(&(request.img), 0, sizeof request.img);

	// DO NOT alloc memory for the input file (request.img.file)
	// The base64 conversion from client input will be done by
	// the JSON library, which allocates memory for the binary output

	// But alloc memory for the rgb conversion
	request.img.rgb = malloc(RGB_LIMIT);
	REQUIRE(request.img.rgb != NULL);

	request.img.rows = malloc(ROWS_LIMIT * sizeof(void *));
	REQUIRE(request.img.rows != NULL);

	// And for some logging
	// Start with an empty string
	request.notes = calloc(1, NOTES_LIMIT);
	REQUIRE(request.notes != NULL);

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
    	&(request.img.file), &(request.img.filelen));

	// The post body is not needed anymore
	free(request.post);

	// Just to be extra safe...
	request.post = NULL;

	// An empty request 
	REQUIRE(request.img.filelen > 0);
	REQUIRE(request.img.file != NULL);

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
		"{errno:%d, width:%d, height:%d}\r\n",
		request.errno, request.img.width, request.img.height
	);

	// Still, relay whether the errno is set
	REQUIRE(request.errno == 0);

	return 1;
}


void carefulfree(void *ptr) {
	if (ptr != NULL)
		free(ptr);
}

int cleanup() {
	// Must check for NULLs because an error could have been relayed to
	// this point (cleanup is the last in the hot potato chain)

	// The JSON parser allocates a big chunk for the base64 conversion
	carefulfree(request.img.file);

	// This was allocated in setup
	carefulfree(request.img.rgb);
	carefulfree(request.img.rows);

	carefulfree(request.notes);

	for (int i = 0; i < PALETTE_N; i++) {
		printf(" #%02X%02X%02X ",
		request.palette[i*3+0],
		request.palette[i*3+1],
		request.palette[i*3+2]);
	}
	printf("\n");
}

///////////////////////////////////////////////////////////////////////


// -- SERVICES --------------------------------------------------------

int service_getimgattributes() {

	HOTPOTATO

	request.ispng = detect_png(request.img.file);
	request.isjpg = detect_jpg(request.img.file);

	if (request.isjpg)
		REQUIRE(loaduserjpg());

	if (request.ispng) {
		REQUIRE(loaduserpng());
		REQUIRE(service_palette());
	}

	// The image shouldn't be too big
	REQUIRE( request.img.height < ROWS_LIMIT );


	return 1;
}

// Use the exoquant library
// https://github.com/exoticorn/exoquant
int service_palette() {

	HOTPOTATO

	exq_data *pExq = exq_init();
	REQUIRE(pExq != NULL);

	exq_no_transparency(pExq);

	exq_feed(pExq, request.img.rgb,
		request.img.width * request.img.height);

	exq_quantize(pExq, PALETTE_N);

	exq_get_palette(pExq, request.palette, PALETTE_N);

	exq_free(pExq);

	return 1;
}

int loaduserjpg() {

	HOTPOTATO

	REQUIRE( read_JPG_file(&(request.img)) == 1 );

	return 1;
}

int loaduserpng() {

	HOTPOTATO

	REQUIRE( read_PNG_file(&(request.img)) == 1 );

	return 1;
}


///////////////////////////////////////////////////////////////////////


// -- COLOURSTRING ------------------------------------------------------------

// Add a note to the log
void makenote(const char *note) {
	strcat(request.notes, note);
	strcat(request.notes, "; ");
}

void potatostack(int linenum) {
	char note[100] = {0};
	sprintf(note, "HOTPOTATO %d", linenum);
	makenote(note);
}


// SECTION PNG

// Check if the stream starts like a PNG file
// 1 means yes, 0 means no.
int detect_png(unsigned char *stream) {

	HOTPOTATO

	return (png_check_sig(stream, 8) == 0)? 0 : 1;
}


struct my_png_err_struct {
	// A callback for saving the error message
	void (*makenote)(const char *note);
	// Where to return
	jmp_buf error_jmp;
};

void my_png_error_callback(png_structp png_ptr,
	png_const_charp error_msg) {

	// Get the error struct
	struct my_png_err_struct *errorcontext =
		png_get_error_ptr(png_ptr);

	// Save the message
	errorcontext->makenote(error_msg);

	// Jump back to the calling function
	longjmp(errorcontext->error_jmp, 1);
}

void my_png_warning_callback(png_structp png_ptr,
	png_const_charp warning_msg) {

	// Get the error struct
	struct my_png_err_struct *errorcontext =
		png_get_error_ptr(png_ptr);

	// Save the message
	errorcontext->makenote(warning_msg);

	// Jump back to the calling function
	longjmp(errorcontext->error_jmp, 1);
}

// PNG user-provided read data callback
void png_user_read(png_structp png_ptr,
	png_bytep dest, png_size_t length) {

	// The file handle
	rasterimagefile *img = png_get_io_ptr(png_ptr);

	// The number of bytes that haven't been read yet
	unsigned int remaining = img->filelen - img->readprogress;

	// Read the lesser of length vs. remaining
	// (could use a min function...)
	unsigned int progress = (length < remaining)? length : remaining;

	// ASSUME THIS WORKS AS EXPECTED EVERY TIME
	memcpy(dest, img->file + img->readprogress, progress);

	// Move forward
	img->readprogress += progress;
}

// PNG user-provided write data callback
void png_user_write(png_structp png_ptr,
	png_bytep data, png_size_t length) {

	// The file handle
	rasterimagefile *img = png_get_io_ptr(png_ptr);

	// Space left in the write stream
	unsigned int remaining = img->rgblimit - img->rgblen;

	// Write the lesser of length vs. remaining
	unsigned int progress = (length < remaining)? length : remaining;

	// ASSUME THIS WORKS
	memcpy(img->rgb + img->rgblen, data, progress);

	// Move forward
	img->rgblen += progress;
}

// PNG user-provided flush data callback
void png_user_flush(png_structp png_ptr) {
	// ....?
}

int read_PNG_file(rasterimagefile *img) {

	png_structp png_ptr;
	png_infop info_ptr;
	png_infop end_info;

	struct my_png_err_struct errorcontext;
	errorcontext.makenote = makenote;

	// Initialize a reading context with error callbacks
	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING,
		(png_voidp) &errorcontext, my_png_error_callback,
		my_png_warning_callback);
	if (png_ptr == NULL) {
		// Nothing to destroy, nothing was created
		REQUIRE(0);
	}

	// Initialize the info interface
	info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL) {
		png_destroy_read_struct(&png_ptr, NULL, NULL);
		REQUIRE(0);
	}
	REQUIRE(info_ptr);

	// The end info interface, which does I don't know what
	end_info = png_create_info_struct(png_ptr);
	if (!end_info) {
		png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
		REQUIRE(0);
	}

	// If the PNG library fails it will jump to here
	if (setjmp(errorcontext.error_jmp)) {
		// Put the jump point here so all created structs
		// get destroyed
		png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
		return 0;
	}

	// Provide the read callback
	png_set_read_fn(png_ptr, img, png_user_read);

	// Set dimension limits
	png_set_user_limits(png_ptr, ROWS_LIMIT, ROWS_LIMIT);

	// Provide the write callback
	//png_set_write_fn(png_ptr, img, png_user_write, png_user_flush);

	// Let unknown chunks get discarded
	// Do not provide a row read event callback
	// Use the default dimensions limit (1 million by 1 million)

	// Read the image header data
	// This function returns void
	png_read_info(png_ptr, info_ptr);

	(*img).width = png_get_image_width(png_ptr, info_ptr);
	(*img).height = png_get_image_height(png_ptr, info_ptr);
	(*img).channels = png_get_channels(png_ptr, info_ptr);

	// Set up the row pointers
	// EXPECT EXACTLY 3 BYTES PER PIXEL
	for (int i = 0; i < (*img).height; i++)
		(*img).rows[i] = (*img).rgb + (i * (*img).width * 3);

	// Read the whole image at once
	png_read_image(png_ptr, (*img).rows);

	// This may not be necessary but it seems neat to do it anyways
	png_read_end(png_ptr, end_info);

	// Free memory we don't need anymore
	png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);

	return 1;
}

// Stashed notes for later use:

/*
// This set function returns void
png_set_write_fn(png_structp write_ptr,
	voidp write_io_ptr, png_rw_ptr write_data_fn,
	png_flush_ptr output_flush_fn);
*/




// END PNG


// SECTION JPEG

struct my_jpeg_error_context {
	struct jpeg_error_mgr pub;  // public fields
	jmp_buf setjmp_buffer;      // for return to caller
};

// Here's the routine that will replace the standard error_exit method:
void my_jpeg_error_callback(j_common_ptr cinfo)
{
	struct my_jpeg_error_context *myerr =
		(struct my_jpeg_error_context *) cinfo->err;

	//(*cinfo->err->output_message) (cinfo);

	// Return control to the setjmp point
	longjmp(myerr->setjmp_buffer, 1);
}


int detect_jpg(unsigned char *stream) {

	HOTPOTATO

	struct jpeg_decompress_struct cinfo;
	struct my_jpeg_error_context jerr;

	cinfo.err = jpeg_std_error(&jerr.pub);
	jerr.pub.error_exit = my_jpeg_error_callback;
	if (setjmp(jerr.setjmp_buffer)) {
		jpeg_destroy_decompress(&cinfo);
		// Not a JPG file
		return 0;
	}
	jpeg_create_decompress(&cinfo);
	
	// Set a fake stream size. Just interested in dimensions
	jpeg_mem_src(&cinfo, stream, 10000);

	(void)jpeg_read_header(&cinfo, TRUE);
	
	(void)jpeg_start_decompress(&cinfo);

	jpeg_destroy_decompress(&cinfo);

	return 1;
}


// Read a JPEG file and write it as a string of RGB values
int read_JPG_file(rasterimagefile *img)
{
	HOTPOTATO

	struct jpeg_decompress_struct cinfo;
	struct my_jpeg_error_context jerr;
	JSAMPARRAY buffer;  // Output row buffer
	int row_stride;     // physical row width in output buffer
	
	cinfo.err = jpeg_std_error(&jerr.pub);
	jerr.pub.error_exit = my_jpeg_error_callback;
	if (setjmp(jerr.setjmp_buffer)) {
		jpeg_destroy_decompress(&cinfo);
		REQUIRE(0);
	}

	jpeg_create_decompress(&cinfo);
	
	jpeg_mem_src(&cinfo, img->file, img->filelen);

	(void)jpeg_read_header(&cinfo, TRUE);
	
	(void)jpeg_start_decompress(&cinfo);

	img->width = cinfo.output_width;
	img->height = cinfo.output_height;
	img->channels = cinfo.output_components;
	
	jpeg_destroy_decompress(&cinfo);
	
	// At this point you may want to check to see whether any corrupt-data
	// warnings occurred (test whether jerr.pub.num_warnings is nonzero).
	
	return 1;
}

// END JPEG

///////////////////////////////////////////////////////////////////////


