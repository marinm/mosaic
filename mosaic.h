#ifndef MOSAIC_H
#define MOSAIC_H

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

	char *file;
	int   binlen;

	int size;
	int w;
	int h;
} request;


int setup();

int copystdin();

int loadrequest();

int loadimage();

int printjson();

int cleanup();

#endif
