// mosaic.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "frozen.h"

int main() {
	unsigned char *post_payload = malloc(5000000);
	size_t payload_len = 0;

	struct {
		int len;
		char *file;
		int b64len;
	} request;
	memset(&request, 0, sizeof request);

	payload_len = fread(post_payload, 1, 5000000, stdin);
	if (payload_len == 0)
		return 0;

	// Make sure the input is null-terminated
	post_payload[payload_len] = '\0';

 	json_scanf(post_payload, payload_len, "{length:%d, file:%V}",
    	&(request.len), &(request.file), &(request.b64len));

	printf("Content-Type: application/json; charset=utf-8;\r\n\r\n");
	printf("{\"len\":%d, \"b64len\":%d}", request.len, request.b64len);
	printf("\r\n");

	free(request.file);
	free(post_payload);
	return 0;
}
