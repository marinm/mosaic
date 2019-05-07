// mosaic.c
// Reads in an image file and returns a transformed image that looks
// like a mosaic. Input and output are both JSON.
//
// SECTIONS IN THIS FILE
//   * Includes
//   * Limits
//   * Global variables
//   * Forward declarations
//   * Main
//   * Mosaic
//   * Services
//   * Colourstring
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

// Limit the size of the output PNG file
#define PNG_LIMIT 1000000

// Limit the capacity of notes (warnings/errors)
#define NOTES_LIMIT 100000

// Number of colours in quantization
#define PALETTE_N 256


// -- GLOBAL VARIABLES ----------------------------------------------------

// The common properties of various file formats representing
// a rectangular array of colour values (i.e. raster image)
typedef struct {
	// The entire file should be loaded in a large array
	unsigned char  *file;
	unsigned int    filelen;
	unsigned long   filelimit;

	unsigned int    width;
	unsigned int    height;
	unsigned int    channels;

	// The entire raster RGB conversion should fit in the
	// allocated array provided (with given limit)
	unsigned char  *rgb;
	unsigned char   rgblen;
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

	// Capture warnings and errors from libraries that operate on this
	// struct. This is not image meta-data. It should only be used for
	// debugging.
	void          (*makenote)(const char *note);

	// Bookkeeping states
	unsigned int    readprogress;
	unsigned int    num_unwritten;
} rasterimagefile;

// For benchmarking
typedef unsigned long long timestamp_t;


// Global/singleton handle
struct {
	char *post;        // The client's complete POST request text
	int   postlen;     // POST payload length

	char *response;    // The complete JSON response 
	int   responselen; // Response length

	rasterimagefile
	      img;         // The user's provided image, and rgb conversion

	rasterimagefile
		  noisy;

	rasterimagefile    // The palette as a 16x16 PNG
	      palettepng;

	int   errno;
	char *notes;

	timestamp_t
	      start;
	timestamp_t
	      finish;
} request;



// -- FORWARD DECLARATIONS ----------------------------------------------

int setup();
int doservice();
int printresponse();
int cleanup();

int free_rasterimagefile(rasterimagefile *img);
int create_rasterimagefile(rasterimagefile *img);

int write_default_palette();

int detect_png(unsigned char *stream);
int detect_jpg(unsigned char *stream);

int loaduserjpg();
int loaduserpng();

int read_JPG_file(rasterimagefile *image);
int read_PNG_file(rasterimagefile *image);

int write_png(rasterimagefile *image);
int palette_to_png();

int copystdin();
int loadrequest();

int make_palette();
int service_getimgattributes();
int service_noisypng();
int service_copy();

void potatostack(int linenum);
void makenote(const char *note);



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


// -- DEVELOPMENT -----------------------------------------------------

// If a function sets the errno flag, future functions should not do anything.
// The hot potato is passed down the stack.
#define FEELPOTATO if (request.errno != 0) {return 0;}

// Requiring makes the potato hot.
// A function should not leave dangling pointers before requiring.
// Note that return values and errno treat 1/0 differently.
#define COOLPOTATOREQUIRES(C)             \
	if (!(C)) {                \
		request.errno++;       \
		potatostack(__LINE__); \
		return 0;              \
	}

// Add a note to the log
void makenote(const char *note) {
#ifdef DEBUG
	strcat(request.notes, note);
	strcat(request.notes, "; ");
#endif
}

// Record which line number a REQUIRE returned on
void potatostack(int linenum) {
#ifdef DEBUG
	char note[100] = {0};
	sprintf(note, "FEELPOTATO %d", linenum);
	makenote(note);
#endif
}

timestamp_t get_timestamp ()
{
  struct timeval now;
  gettimeofday(&now, NULL);
  return now.tv_usec + (timestamp_t)now.tv_sec * 1000000;
}


// -- MOSAIC ----------------------------------------------------------

int setup() {
	// Zero out the request context
	memset(&request, 0, sizeof request);

	// Start the stopwatch...
	request.start = get_timestamp();

	// The errno indicates if future functions should even execute.
	request.errno = 0;


	// And for some logging
	// Start with an empty string
	request.notes = calloc(1, NOTES_LIMIT);
	COOLPOTATOREQUIRES(request.notes != NULL);

	// The image objects
	COOLPOTATOREQUIRES( create_rasterimagefile(&request.img) );
	COOLPOTATOREQUIRES( create_rasterimagefile(&request.palettepng) );

	// If the request image file cannot be parsed, then return
	// a default palette
	COOLPOTATOREQUIRES(write_default_palette());

	// Start processing the request...
	COOLPOTATOREQUIRES(loadrequest());

	return 1;
}


// Copy the POST payload to memory and parse the JSON contents
int loadrequest() {

	FEELPOTATO

	// Copy the POST payload to memory
	COOLPOTATOREQUIRES(copystdin());

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
	COOLPOTATOREQUIRES(request.img.filelen > 0);
	COOLPOTATOREQUIRES(request.img.file != NULL);

	return 1;
}



// Copy all of stdin to memory, up to some limit
// This function creates a new memory block.
int copystdin() {

	FEELPOTATO

	// Allocate a big block of memory
	request.post = malloc(POST_LIMIT);

	COOLPOTATOREQUIRES(request.post != NULL);

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

	FEELPOTATO

	// Read image dimensions and palette
	COOLPOTATOREQUIRES(service_getimgattributes());

	if (request.img.ispng)
		// Create a palette image and convert to PNG file
		COOLPOTATOREQUIRES(palette_to_png());

	return 1;
}

int printresponse() {

	// Stop the stopwatch...
	request.finish = get_timestamp();

	// Run time up till now, in milliseconds
    unsigned long millisec = (request.finish - request.start) / 1000L;

	// Write the response to stdout
	printf("Content-Type: application/json; charset=utf-8;\r\n\r\n");

	struct json_out out = JSON_OUT_FILE(stdout);
	request.responselen = json_printf(&out,
		"{errno:%d, ptime:%lu, width:%d, height:%d, palettepng:%V}\r\n",
		request.errno, millisec, request.img.width, request.img.height,
		request.palettepng.file, request.palettepng.filelen
	);

#ifdef DEBUG
	printf("NOTES: %s\n", request.notes);
	printf("unwritten %d\n", request.palettepng.num_unwritten);
#endif

	// Still, relay whether the errno is set
	COOLPOTATOREQUIRES(request.errno == 0);

	return 1;
}


int cleanup() {
	// The JSON parser allocates a big chunk for the base64 conversion

	// The file stream in every rasterimagefile object is allocated
	// externally, but freed by free_rasterimagefile
	free_rasterimagefile(&(request.img));
	free_rasterimagefile(&(request.palettepng));

	free(request.notes);
}



// -- SERVICES --------------------------------------------------------

int write_default_palette() {
	for (int i = 0; i < PALETTE_N; i++)
		request.img.palette[i] = 0xF0;

	for (int i = 0; i < PALETTE_N; i++)
		request.palettepng.palette[i] = 0xF0;
}

int service_getimgattributes() {

	FEELPOTATO

	request.img.ispng = detect_png(request.img.file);
	request.img.isjpg = detect_jpg(request.img.file);

	if (request.img.isjpg)
		COOLPOTATOREQUIRES(loaduserjpg());

	if (request.img.ispng) {
		COOLPOTATOREQUIRES(loaduserpng());
		COOLPOTATOREQUIRES(make_palette(&request.img));
	}

	// The image shouldn't be too big
	COOLPOTATOREQUIRES( request.img.height < ROWS_LIMIT );

	return 1;
}

// Use the exoquant library
// https://github.com/exoticorn/exoquant
int make_palette(rasterimagefile *img) {

	FEELPOTATO

	// ** This library works only with RGBA streams **
	// ** Need to make a temporary copy of the data **
	unsigned long npixels = img->width * img->height;
	unsigned char *rgba = malloc(npixels * 4);
	COOLPOTATOREQUIRES(rgba != NULL);

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

int service_noisypng() {

	FEELPOTATO

	COOLPOTATOREQUIRES( write_png(NULL) == 1 );

	return 1;
}

int loaduserjpg() {

	FEELPOTATO

	COOLPOTATOREQUIRES( read_JPG_file(&(request.img)) == 1 );

	return 1;
}

int loaduserpng() {

	FEELPOTATO

	COOLPOTATOREQUIRES( read_PNG_file(&(request.img)) == 1 );

	return 1;
}

// Create a 16x16 raster image file (PNG) where each of the 256 pixels are
// from the palette
int palette_to_png() {

	// Initialize
	request.palettepng.filelen = 0;
	request.palettepng.file = malloc(request.palettepng.filelimit);
	COOLPOTATOREQUIRES(request.palettepng.file != NULL);

	// Copy the palette from the client image to the palettepng image
	memcpy(request.palettepng.palette, request.img.palette,
		request.img.palettecount * 3);

	// Create the palette RGB string
	// ** Assume the palette is 8-bit RGB **
	for (int i = 0; i < 256; i++)
		request.palettepng.rgb[i] = (unsigned char) i;

	// Set up the row pointers
	for (int i = 0; i < 16; i++)
		request.palettepng.rows[i] = request.palettepng.rgb + (16*i);

	request.palettepng.width = 16;
	request.palettepng.height = 16;
	request.palettepng.channels = 3;

	COOLPOTATOREQUIRES( write_png(&request.palettepng) );

	return 1;
}


///////////////////////////////////////////////////////////////////////


// -- COLOURSTRING --------------------------------------------------------


// Create a malloc'd rasterimagefile object and also malloc all inner
// arrays to some pre-set capacity.
int create_rasterimagefile(rasterimagefile *newimg) {

	// Zero out the raster image file handle
	memset(newimg, 0, sizeof(rasterimagefile));

	// Explicity NULLing it here as a reminder that the file should
	// come from elsewhere.
	newimg->file = NULL;
	newimg->filelen = 0;

	// Some pre-set fixed values
	newimg->channels = 3;
	newimg->rgblimit = RGB_LIMIT;
	newimg->filelimit = PNG_LIMIT;
	newimg->makenote = makenote;
	newimg->palettecount = PALETTE_N;

	// But alloc memory for the rgb conversion

	// RGB string of user image
	newimg->rgb = calloc(RGB_LIMIT, 1);
	if (newimg->rgb == NULL) {
		free_rasterimagefile(newimg);
		return 0;
	}

	newimg->rows = calloc(ROWS_LIMIT * sizeof(void *), 1);
	if (newimg->rows == NULL) {
		free_rasterimagefile(newimg);
		return 0;
	}

	newimg->palette = calloc(PALETTE_N * 4, 1);
	if (newimg->palette == NULL) {
		free_rasterimagefile(newimg);
		return 0;
	}

	// Everything worked.

	return 1;
}

int free_rasterimagefile(rasterimagefile *img) {
	if (img == NULL)
		return 1;
	free(img->file);
	free(img->rgb);
	free(img->rows);
	free(img->palette);
	return 1;
}


// SECTION PNG

// Check if the stream starts like a PNG file
// 1 means yes, 0 means no.
int detect_png(unsigned char *stream) {

	FEELPOTATO

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
		COOLPOTATOREQUIRES(0);
	}

	// Initialize the info interface
	info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL) {
		png_destroy_read_struct(&png_ptr, NULL, NULL);
		COOLPOTATOREQUIRES(0);
	}
	COOLPOTATOREQUIRES(info_ptr);

	// The end info interface, which does I don't know what
	end_info = png_create_info_struct(png_ptr);
	if (!end_info) {
		png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
		COOLPOTATOREQUIRES(0);
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

// PNG user-provided write data callback
void png_user_write(png_structp png_ptr,
	png_bytep data, png_size_t length) {

	// The file handle
	rasterimagefile *img = png_get_io_ptr(png_ptr);

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

// Writes a rasterimagefile to PNG.
// ** The rgb stream is read as 8-bit palette indices **
// ** Output is always in the 256-colour space **
// Returns 1 on completion, 0 on quit before completion
int write_png(rasterimagefile *img) {

	FEELPOTATO

	struct my_png_err_struct errorcontext;
	errorcontext.makenote = makenote;

	png_structp png_ptr;
	png_infop info_ptr;

	png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,
		(png_voidp) &errorcontext , my_png_error_callback, my_png_warning_callback);
	if (png_ptr == NULL)
		// Nothing to destroy
		COOLPOTATOREQUIRES(0);

	info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL) {
		png_destroy_write_struct(&png_ptr, (png_infopp) NULL);
		COOLPOTATOREQUIRES(0);
	}

	// Set the jump point. Indicate quit before completion.
	if (setjmp(errorcontext.error_jmp)) {
		png_destroy_write_struct(&png_ptr, &info_ptr);
		COOLPOTATOREQUIRES(0);
	}

	// Set header values
	// ** Always using 8-bit-depth colour values **
	png_set_IHDR(png_ptr, info_ptr,
		img->width, img->height, 8,
		PNG_COLOR_TYPE_PALETTE,
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
	png_set_PLTE(png_ptr, info_ptr, (png_colorp) img->palette,
		img->palettecount);

	// Set the data
	png_set_rows(png_ptr, info_ptr, img->rows);

	img->num_unwritten = 0;

	// ** How to check if this worked? **
	png_write_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL);

	// Finish
	png_destroy_write_struct(&png_ptr, &info_ptr);

	return 1;
}
// Stashed notes for later use:

/*
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

	FEELPOTATO

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
	FEELPOTATO

	struct jpeg_decompress_struct cinfo;
	struct my_jpeg_error_context jerr;
	JSAMPARRAY buffer;  // Output row buffer
	int row_stride;     // physical row width in output buffer
	
	cinfo.err = jpeg_std_error(&jerr.pub);
	jerr.pub.error_exit = my_jpeg_error_callback;
	if (setjmp(jerr.setjmp_buffer)) {
		jpeg_destroy_decompress(&cinfo);
		COOLPOTATOREQUIRES(0);
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


