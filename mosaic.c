// mosaic.c
//
// Input: {"file":"..."}
// The "file" string is a base64-encoded PNG image.
//
// Output: under construction...
//
// SECTIONS IN THIS FILE
//   * Includes
//   * Limits
//   * Master struct, Global variables
//   * Forward declarations
//   * Main
//   * Stages
//   * rif routines
//

// -- INCLUDES ------------------------------------------------------------

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <sys/time.h>

#include "jpeglib.h"
#include "png.h"
#include "exoquant.h"
#include "frozen.h"

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize.h"


// -- LIMITS --------------------------------------------------------------

// Limit the size of the client's request text
#define POST_LIMIT 5000000

// Maximum image height
#define ROWS_LIMIT 4000

// Limit the size of the RGB expansion
// The RGB expansion is almost certainly more than 3x larger
// than the input file
// 4000*4000*3 = 48000000
#define RGB_LIMIT 100000000

// Limit the size of the output file
#define FILE_LIMIT 10000000

// Limit the capacity of notes (warnings/errors)
#define NOTES_LIMIT 100000

// Number of colours in quantization
#define PALETTE_N 256

// Resize image to these dimensions
#define FIT_W 400
#define FIT_H 400

// -- GLOBAL VARIABLES ----------------------------------------------------

// File types: PNG, JPEG, other
enum {RIF_FTYPE_PNG, RIF_FTYPE_JPEG, RIF_FTYPE_OTHER};

// rif -- raster image file
// The common properties of various file formats representing
// a rectangular array of colour values
typedef struct {
	// The entire file should be loaded in a large array
	unsigned char  *file;
	unsigned long   filelen;
	unsigned long   filelimit;

	// Whether the file is PNG, JPEG, or other
	unsigned int    fileformat;

	unsigned int    width;
	unsigned int    height;
	unsigned int    channels;

	// Enumerated colourspace (grayscale, RGB, etc.)
	unsigned int    colorspace;

	// The entire raster RGB conversion should fit in the
	// allocated array provided (with given limit)
	unsigned char  *rgb;
	unsigned long   rgblen;
	unsigned long   rgblimit;
	unsigned char **rows;
	// rgblen = width*height*components

	// The palette is another string of RGB values,
	// but much shorter
	unsigned char  *palette;
	unsigned int    palettecount;

	// Detect file type
	int             ispng;
	int             isjpg;

	// Bookkeeping states
	unsigned int    readprogress;
	unsigned int    num_unread;
	unsigned int    num_unwritten;
} rif;


// For benchmarking
typedef unsigned long long timestamp_t;


// Master struct
struct {
	unsigned char  *post;        // The client's complete POST request text
	unsigned long   postlen;     // POST payload length

	unsigned char  *response;    // The complete JSON response 
	unsigned long   responselen; // Response length

	rif             img;         // The user's provided image, and rgb conversion
	rif             resized;

	unsigned int    errno;

	timestamp_t     start;
	timestamp_t     finish;
} request;



// -- FORWARD DECLARATIONS ----------------------------------------------

int setup();
int doservice();
int printresponse();
int cleanup();

int free_rif(rif *img);
int create_rif(rif *img);

int unpack_rif(rif *image);
int read_JPG_file(rif *image);
int read_PNG_file(rif *image);
int write_JPG_file(rif *img);
int write_png(rif *image);
int rifresize(rif *img, rif *resized);

int copystdin();
int loadrequest();

int make_palette();
int getimgattributes();
int service_copy();
int imgresize();


void potatostack(int linenum);


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


// -- DEVELOPMENT -----------------------------------------------------

// If a function sets the errno flag, future functions should not do
// anything. The hot potato is passed along.
#define FEELPOTATO \
		if (request.errno != 0) {return 0;}

#define HOTPOTATO                     \
		if (request.errno == 0)       \
			request.errno = __LINE__; \
		return 0;

// Requiring makes the potato hot.
// A function should not leave dangling pointers before requiring.
// Note that return values and errno treat 1/0 differently.
#define CHECK(C)  if (!(C)) { HOTPOTATO }

timestamp_t get_timestamp ()
{
	struct timeval now;
	gettimeofday(&now, NULL);
	return now.tv_usec + (timestamp_t)now.tv_sec * 1000000;
}


// -- STAGES ----------------------------------------------------------

int setup() {
	// Zero out the request context
	memset(&request, 0, sizeof request);

	// Start the stopwatch...
	request.start = get_timestamp();

	// The errno indicates if future functions should even execute.
	request.errno = 0;

	// The image objects
	CHECK( create_rif(&request.img) );
	CHECK( create_rif(&request.resized) );

	// Start processing the request...
	CHECK( loadrequest() );

	return 1;
}


// Copy the POST payload to memory and parse the JSON contents
int loadrequest() {

	FEELPOTATO

	// Copy the POST payload to memory
	CHECK(copystdin());

	// Parse JSON
	// The parser allocates memory for its output
 	json_scanf(request.post, request.postlen, "{file:%V}",
    	&(request.img.file), &(request.img.filelen));

	// Resize the file memory block to capacity
	request.img.file = realloc(request.img.file, FILE_LIMIT);

	// The post body is not needed anymore
	// Null it out just to be safe
	free(request.post);
	request.post = NULL;

	// An empty request 
	CHECK( request.img.filelen > 0 );
	CHECK( request.img.file != NULL );

	return 1;
}


// Copy all of stdin to memory, up to some limit
// This function creates a new memory block.
int copystdin() {

	FEELPOTATO

	// Allocate a big block of memory
	request.post = malloc(POST_LIMIT);
	CHECK(request.post != NULL);

	// Copy entire POST payload to memory from stdin.
	// This is simplistic but works most of the time.
	// fread does not guarantee that it will read until EOF.
	request.postlen = fread(request.post, 1, POST_LIMIT, stdin);
	assert(request.postlen != 0);

	// Make sure the input is null-terminated
	request.post[request.postlen] = '\0';

	return 1;
}


int doservice() {

	FEELPOTATO

	// Read image dimensions and palette
	CHECK(getimgattributes());

	CHECK(imgresize());
	CHECK(write_JPG_file(&request.img));
	return 1;
}

int printresponse() {

	// Stop the stopwatch...
	request.finish = get_timestamp();

	// Run time up till now, in milliseconds
    unsigned long millisec = (request.finish - request.start) / 1000L;

#ifndef JSON_ONLY
	// Write the response to stdout
	printf("Content-Type: application/json; charset=utf-8;\r\n\r\n");
#endif

	struct json_out out = JSON_OUT_FILE(stdout);
	request.responselen = json_printf(&out,
		"{errno:%d, ptime:%lu, width:%d, height:%d, rgblen:%d, img:%V}\r\n",
		request.errno, millisec, request.img.width, request.img.height,
		request.img.rgblen,
		request.img.file, request.img.filelen
	);

	// Still, relay whether the errno is set
	CHECK(request.errno == 0);

	return 1;
}


int cleanup() {
	// The JSON parser allocates a big chunk for the base64 conversion

	// The file stream in every rif object is allocated
	// externally, but freed by free_rif
	free_rif(&(request.img));
}


// -- RASTERIMAGEFILE ROUTINES --------------------------------------------

int getimgattributes() {
	FEELPOTATO
	CHECK( unpack_rif(&request.img) );
	return 1;
}

int imgresize() {
	FEELPOTATO
	request.resized.width = FIT_W;
	request.resized.height = FIT_H;
	CHECK( rifresize(&request.img, &request.resized) );
	return 1;
}

// Use the exoquant library
// https://github.com/exoticorn/exoquant
int make_palette(rif *img) {

	FEELPOTATO

	// ** This library works only with RGBA streams **
	// ** Need to make a temporary copy of the data **
	unsigned long npixels = img->width * img->height;
	unsigned char *rgba = malloc(npixels * 4);
	CHECK(rgba != NULL);

	// Also need a separate copy of the palette
	// It will need to be stripped of the alpha channel
	unsigned char *palette = malloc(npixels * 4);
	if (palette == NULL) {
		free(rgba);
		return 0;
	}

	exq_data *pExq = exq_init();
	if (pExq == NULL) {
		free(rgba);
		return 0;
	}

	// Expand RGB to RGBA
	for (int i = 0; i < npixels; i++) {
		rgba[i*4 + 0] = img->rgb[i*3 + 0];
		rgba[i*4 + 1] = img->rgb[i*3 + 1];
		rgba[i*4 + 2] = img->rgb[i*3 + 2];
		rgba[i*4 + 3] = 0xFF;
	}

	exq_no_transparency(pExq);
	exq_feed(pExq, rgba, npixels);

	exq_quantize(pExq, img->palettecount);
	exq_get_palette(pExq, palette, img->palettecount);
	exq_free(pExq);

	// Copy the palette and strip the alpha channel
	for (int i = 0; i < img->palettecount; i++) {
		img->palette[i*3 + 0] = palette[i*4 + 0];
		img->palette[i*3 + 1] = palette[i*4 + 1];
		img->palette[i*3 + 2] = palette[i*4 + 2];
	}

	free(palette);
	free(rgba);

	return 1;
}

// -- RIF -- RASTER IMAGE FILE --------------------------------------------

// Create a malloc'ed rif object and also malloc all inner
// arrays to some pre-set capacity.
int create_rif(rif *newimg) {

	// Zero out the raster image file handle
	memset(newimg, 0, sizeof(rif));

	// Explicity NULLing it here as a reminder that the file should
	// come from elsewhere.
	newimg->file = NULL;
	newimg->filelen = 0;

	// Some pre-set fixed values
	newimg->channels = 3;
	newimg->rgblimit = RGB_LIMIT;
	newimg->filelimit = FILE_LIMIT;
	newimg->palettecount = PALETTE_N;

	// But alloc memory for the rgb conversion

	// RGB string of user image
	newimg->rgb = calloc(RGB_LIMIT, 1);
	if (newimg->rgb == NULL) {
		free_rif(newimg);
		return 0;
	}

	newimg->rows = calloc(ROWS_LIMIT * sizeof(void *), 1);
	if (newimg->rows == NULL) {
		free_rif(newimg);
		return 0;
	}

	newimg->palette = calloc(PALETTE_N * 4, 1);
	if (newimg->palette == NULL) {
		free_rif(newimg);
		return 0;
	}

	// Everything worked.

	return 1;
}

int free_rif(rif *img) {
	if (img == NULL)
		return 1;
	free(img->file);
	free(img->rgb);
	free(img->rows);
	free(img->palette);
	return 1;
}


int unpack_rif(rif *img) {
	if (read_JPG_file(img) == 1)
		return 1;
	if (read_PNG_file(img) == 1)
		return 1;
	return 0;
}

// SECTION PNG

struct my_png_err_struct {
	// Where to return
	jmp_buf error_jmp;
};

void my_png_error_callback(png_structp png_ptr,
	png_const_charp error_msg) {

	// Get the error struct
	struct my_png_err_struct *errorcontext =
		png_get_error_ptr(png_ptr);

	// Jump back to the calling function
	longjmp(errorcontext->error_jmp, 1);
}

void my_png_warning_callback(png_structp png_ptr,
	png_const_charp warning_msg) {

	// Get the error struct
	struct my_png_err_struct *errorcontext =
		png_get_error_ptr(png_ptr);

	// Jump back to the calling function
	longjmp(errorcontext->error_jmp, 1);
}



// PNG user-provided read data callback
void png_user_read(png_structp png_ptr,
	png_bytep dest, png_size_t length) {

	// The file handle
	rif *img = png_get_io_ptr(png_ptr);

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


int read_PNG_file(rif *img) {

	png_structp png_ptr;
	png_infop info_ptr;
	png_infop end_info;

	unsigned int row_stride;

	struct my_png_err_struct errorcontext;

	// Initialize a reading context with error callbacks
	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING,
		(png_voidp) &errorcontext, my_png_error_callback,
		my_png_warning_callback);
	if (png_ptr == NULL) {
		// Nothing to destroy, nothing was created
		HOTPOTATO
	}

	// Initialize the info interface
	info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL) {
		png_destroy_read_struct(&png_ptr, NULL, NULL);
		HOTPOTATO
	}
	CHECK(info_ptr);

	// The end info interface, which does I don't know what
	end_info = png_create_info_struct(png_ptr);
	if (!end_info) {
		png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
		HOTPOTATO
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

	// Let unknown chunks get discarded
	// Do not provide a row read event callback
	// Use the default dimensions limit (1 million by 1 million)

	// Before starting reading...
	(*img).readprogress = 0;

	// Read the image header data
	// This function returns void
	png_read_info(png_ptr, info_ptr);

	(*img).width = png_get_image_width(png_ptr, info_ptr);
	(*img).height = png_get_image_height(png_ptr, info_ptr);
	(*img).channels = png_get_channels(png_ptr, info_ptr);

	row_stride = (*img).width * 3;

	// Set up the row pointers
	// EXPECT EXACTLY 3 BYTES PER PIXEL
	for (int i = 0; i < (*img).height; i++)
		(*img).rows[i] = (*img).rgb + (i * row_stride);

	png_set_expand(png_ptr);

	// Read the whole image at once
	png_read_image(png_ptr, (*img).rows);

	img->rgblen = (*img).height * row_stride;

	// This may not be necessary but it seems neat to do it anyways
	png_read_end(png_ptr, end_info);

	// Free memory we don't need anymore
	png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);

	return 1;
}

// PNG user-provided write data callback
void png_user_write(png_structp png_ptr,
	png_bytep data, png_size_t length) {

	// The file handle
	rif *img = png_get_io_ptr(png_ptr);

	// Space left in the write stream
	unsigned int remaining = img->filelimit - img->filelen;

	// Write the lesser of length vs. remaining
	unsigned int progress = (length < remaining)? length : remaining;

	// If the limit was exceeded, keep track of how many bytes
	// are being discarded
	if (length > remaining) {
		img->num_unwritten += length - remaining;
		return;
	}

	// ASSUME THIS WORKS
	memcpy(img->file + img->filelen, data, progress);

	// Move forward
	img->filelen += progress;
}


// PNG user-provided flush data callback
void png_user_flush(png_structp png_ptr) {
	// ....?
}

// Writes a rif to PNG.
// ** The rgb stream is read as 8-bit palette indices **
// ** Output is always in the 256-colour space **
// Returns 1 on completion, 0 on quit before completion
int write_png(rif *img) {

	FEELPOTATO

	struct my_png_err_struct errorcontext;

	png_structp png_ptr;
	png_infop info_ptr;

	png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,
		(png_voidp) &errorcontext , my_png_error_callback, my_png_warning_callback);
	if (png_ptr == NULL) {
		// Nothing to destroy
		HOTPOTATO
	}

	info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL) {
		png_destroy_write_struct(&png_ptr, (png_infopp) NULL);
		HOTPOTATO
	}

	// Set the jump point. Indicate quit before completion.
	if (setjmp(errorcontext.error_jmp)) {
		png_destroy_write_struct(&png_ptr, &info_ptr);
		return 0;
	}

	// Set header values
	// ** Always using 8-bit-depth colour values **
	png_set_IHDR(png_ptr, info_ptr,
		img->width, img->height, 8,
		PNG_COLOR_TYPE_RGB,
		PNG_INTERLACE_NONE,
		PNG_COMPRESSION_TYPE_DEFAULT,
		PNG_FILTER_TYPE_DEFAULT);

	// This function returns void
	png_set_write_fn(png_ptr, (png_voidp) img, png_user_write,
		png_user_flush);

	// Set the compression level
	png_set_compression_level(png_ptr, PNG_Z_DEFAULT_COMPRESSION);

	// Set the palette. png_color_struct is just a char-3-tuple R,G,B
	// ** palettecount should be 8 **
	//png_set_PLTE(png_ptr, info_ptr, (png_colorp) img->palette,
	//		img->palettecount);

	// Set the data
	png_set_rows(png_ptr, info_ptr, img->rows);

	img->filelen = 0;
	img->num_unwritten = 0;

	// ** How to check if this worked? **
	png_write_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL);

	// Finish
	png_destroy_write_struct(&png_ptr, &info_ptr);

	return 1;
}

// END PNG


// SECTION JPEG

// jpeglib error context
struct jerrcontext {
	struct jpeg_error_mgr pub;
	jmp_buf jmpmark;
	rif *img;
};

// Here's the routine that will replace the standard error_exit method:
void jerrcallback(j_common_ptr cinfo)
{
	struct jerrcontext *jerr = (struct jerrcontext *) cinfo->err;

#ifdef DEBUG
	rif *img  = jerr->img;
	printf("JPEG error callback: decompressor? (%d) global state (%d), %lu %lu\n",
		cinfo->is_decompressor, cinfo->global_state, img->filelen, img->rgblen);
#endif

	// Return control to the setjmp point
	longjmp(jerr->jmpmark, 1);
}


int copy_jpeg_row(rif *img,
	unsigned char *data, unsigned long length) {

	// Space left in the write stream
	unsigned int remaining = img->rgblimit - img->rgblen;

	// Write the lesser of length vs. remaining
	unsigned int progress = (length < remaining)? length : remaining;

	// If the limit was exceeded, keep track of how many bytes
	// are being discarded
	if (length > remaining) {
		img->num_unread += length - remaining;
		return 0;
	}

	// ASSUME THIS WORKS
	memcpy(img->rgb + img->rgblen, data, progress);

	// Move forward
	img->rgblen += progress;

	return 1;
}

// Read a JPEG file and write it as a string of RGB values
int read_JPG_file(rif *img)
{
	// jpeglib interface
	struct jpeg_decompress_struct cinfo;

	// Error manager
	struct jerrcontext jerr;
	jerr.img = img;

	// Physical row width in output buffer
	int row_stride;

	// Override the default error routine	
	cinfo.err = jpeg_std_error(&jerr.pub);
	jerr.pub.error_exit = jerrcallback;
	if (setjmp(jerr.jmpmark)) {
		jpeg_destroy_decompress(&cinfo);
		return 0;
	}

	// Initialize a decompression instance
	// Do this after setting the error callbacks
	jpeg_create_decompress(&cinfo);

	// Point to the input data (a complete, contiguous JFIF file)	
	jpeg_mem_src(&cinfo, img->file, img->filelen);

	// Read image properties (dimensions, etc.)
	if (jpeg_read_header(&cinfo, TRUE) != JPEG_HEADER_OK) {
		jpeg_destroy_decompress(&cinfo);
		HOTPOTATO
	}

	// Set decompression parameters
	//cinfo.scale_num = 1;
	//cinfo.quantize_colors = TRUE;
	//cinfo.desired_number_of_colors = 16;

	// Start decompression instance
	(void)jpeg_start_decompress(&cinfo);

	// Copy the properties. These dimensions refer to the original
	// image, not necessarily the output if scaling is used
	img->width      = cinfo.output_width;
	img->height     = cinfo.output_height;
	img->channels   = cinfo.output_components;
	img->colorspace = cinfo.out_color_space;

	// Row physical size
	row_stride = cinfo.output_width * cinfo.output_components;

	// Read one row at a time
	while (cinfo.output_scanline < cinfo.output_height) {
		// jpeg_read_scanlines expects an array of pointers to scanlines.
		// Here the array is only one element long, but you could ask for
		// more than one scanline at a time if that's more convenient.

		// Put the row of pixels here
		JSAMPROW dest = img->rgb + img->rgblen;
		(void) jpeg_read_scanlines(&cinfo, &dest, 1);
		img->rgblen += row_stride;
	}

	(void) jpeg_finish_decompress(&cinfo);
	(void) jpeg_destroy_decompress(&cinfo);
	
	// At this point you may want to check to see whether any corrupt-data
	// warnings occurred (test whether jerr.pub.num_warnings is nonzero).

	// The image was read completely
	img->isjpg = 1;
	
	return 1;
}


// Initialize destination.  This is called by jpeg_start_compress()
// before any data is actually written. It must initialize
// next_output_byte and free_in_buffer. free_in_buffer must be
// initialized to a positive value.
void jpeg_init_destination (j_compress_ptr cinfo) {
	rif *img = cinfo->client_data;

	img->filelen = 0;
	cinfo->dest->next_output_byte = img->file;
	cinfo->dest->free_in_buffer = img->filelimit;
}

// This is called whenever the buffer has filled (free_in_buffer
// reaches zero).  In typical applications, it should write out the
// *entire* buffer (use the saved start address and buffer length;
// ignore the current state of next_output_byte and free_in_buffer).
// Then reset the pointer & count to the start of the buffer, and
// return TRUE indicating that the buffer has been dumped.
// free_in_buffer must be set to a positive value when TRUE is
// returned.  A FALSE return should only be used when I/O suspension is
// desired.
boolean jpeg_empty_output_buffer (j_compress_ptr cinfo) {

	// Do nothing. The entire available memory block has been used
	// if this function is called.
	return true;
}

// Terminate destination --- called by jpeg_finish_compress() after all
// data has been written.  In most applications, this must flush any
// data remaining in the buffer.  Use either next_output_byte or
// free_in_buffer to determine how much data is in the buffer.
void jpeg_term_destination (j_compress_ptr cinfo) {
	rif *img = cinfo->client_data;

	img->filelen = img->filelimit - cinfo->dest->free_in_buffer;
}

int jpeg_set_dest_mgr(j_compress_ptr cinfo,
	struct jpeg_destination_mgr *dest, rif *img) {

	dest->next_output_byte    = img->file;
	dest->free_in_buffer      = img->filelimit;	
	dest->init_destination    = jpeg_init_destination;
	dest->empty_output_buffer = jpeg_empty_output_buffer;
	dest->term_destination    = jpeg_term_destination;

	cinfo->client_data = img;
	cinfo->dest = dest;
	return 1;
}

int write_JPG_file(rif *img) {
	// jpeglib interface
	struct jpeg_compress_struct cinfo;

	// Error manager
	struct jerrcontext jerr;
	jerr.img = img;

	// Destination manager
	struct jpeg_destination_mgr destmgr;

	// Pointer to a single row
	JSAMPROW row_pointer[1];

	// Physical row width in output buffer
	int row_stride;

	// Override the default error routine	
	cinfo.err = jpeg_std_error(&jerr.pub);
	jerr.pub.error_exit = jerrcallback;
	if (setjmp(jerr.jmpmark)) {
		jpeg_destroy_compress(&cinfo);
		return 0;
	}

	// Initialize a compression instance
	// Do this after setting the error callbacks
	(void) jpeg_create_compress(&cinfo);

	// Set the output destination manager (user call)
	jpeg_set_dest_mgr(&cinfo, &destmgr, img);
	
	// Describe the pixel format
	cinfo.image_width = img->width;
	cinfo.image_height = img->height;
	cinfo.input_components = 3;
	cinfo.in_color_space = JCS_RGB;

	// Set all defaults after setting color space
	(void) jpeg_set_defaults(&cinfo);

	// Initialize the compression process
	(void) jpeg_start_compress(&cinfo, TRUE);
	// The second parameter tells whether a complete JPEG interchange
	// datastream should be written. This is true in most cases.

	// JSAMPLEs per row in buffer
	row_stride = cinfo.image_width * cinfo.input_components;

	// jpeg_abort() should be called before finishing if not all of the
	// scanlines have been processed
	while (cinfo.next_scanline < cinfo.image_height) {
	    row_pointer[0] = img->rgb + (cinfo.next_scanline * row_stride);
		jpeg_write_scanlines(&cinfo, row_pointer, 1);
	}

	// Were all scanlines written?
	if (cinfo.next_scanline < cinfo.image_height) {
		(void) jpeg_destroy_compress(&cinfo);
		return 0;
	}

	// Complete the compression cycle. This step ensures that the last
	// bufferload of data is written to the destination. It also
	// releases working memory for this instance.
	(void) jpeg_finish_compress(&cinfo);
	(void) jpeg_destroy_compress(&cinfo);

	return 1;
}

// END JPEG

// The first RIF has the RGB stream. The second RIF has enough space for the
// RGB stream, and the width and height preset to what the first one should be
// resized to.
int rifresize(rif *img, rif *resized) {
	
	int num_channels = 3;	
	int returnstatus = 0;

	// Resize the image
	returnstatus = stbir_resize_uint8(img->rgb, img->width, img->height,
		img->width * num_channels, resized->rgb, resized->width, resized->height,
		resized->width * num_channels, num_channels);

	return (returnstatus == 1);
}
