// mosaic.c

#include <stdio.h>

int main() {
	printf("Content-Type: application/json; charset=utf-8;\r\n\r\n");
	printf("{\"returnstatus\":\"OK\"}");
	printf("\r\n");
	return 0;
}
