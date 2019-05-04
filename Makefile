# Makefile

CCSTD=-std=c11

all: server client

server: main.o mosaic.o colourstring.o
	gcc main.o mosaic.o colourstring.o -lfrozen -lpng -ljpeg -o mosaic.cgi

main.o: main.c mosaic.h
	gcc main.c -c -o main.o

mosaic.o: mosaic.c mosaic.h
	gcc mosaic.c -c -o mosaic.o

colourstring.o: colourstring.c colourstring.h
	gcc colourstring.c -ljpeg -c -o colourstring.o


client: mosaic.html mosaic.css mosaic.js mosaic.cgi
	cp -u mosaic.cgi  /var/www/cgi
	cp -u mosaic.html /var/www/html
	cp -u mosaic.css  /var/www/css
	cp -u mosaic.js   /var/www/js
