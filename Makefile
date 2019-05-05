# Makefile

CCSTD=-std=c11

all: server client

server: mosaic.c
	gcc mosaic.c -lfrozen -lpng -ljpeg -g -o mosaic.cgi

client: mosaic.html mosaic.css mosaic.js mosaic.cgi
	cp -u mosaic.cgi   /var/www/cgi
	cp -u mosaic.html  /var/www/html
	cp -u mosaic.css   /var/www/css
	cp -u mosaic.js    /var/www/js
