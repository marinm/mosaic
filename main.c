#include "mosaic.h"

int main() {
	// Prepare memory
	setup();

	// Copy the POST payload to memory
	copystdin();

	// Parse JSON contents, then free the POST copy
	loadrequest();

	// Serealize the image to an RGB sequence
	loadimage();

	// Print the results
	printjson();

	// Free memory
	cleanup();

	return 0;
}
