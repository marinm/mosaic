# Makefile

CCSTD=-std=c11

all: server client

server: mosaic.c pixelarray.c
	gcc mosaic.c pixelarray.c $(CCSTD) -g -lm -lfrozen -ljpeg -o /var/www/cgi/mosaic.cgi

client: mosaic.html mosaic.css mosaic.js
	cp -u mosaic.html /var/www/html
	cp -u mosaic.css  /var/www/css
	cp -u mosaic.js   /var/www/js
