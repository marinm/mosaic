# Makefile

CCSTD=-std=c11

all: server client

server: main.o mosaic.o
	gcc main.o mosaic.o $(CCSTD) -g -lfrozen -ljpeg -o /var/www/cgi/mosaic.cgi

main.o: main.c mosaic.h
	gcc main.c -c -o main.o

mosaic.o: mosaic.c mosaic.h
	gcc mosaic.c -c -o mosaic.o

client: mosaic.html mosaic.css mosaic.js
	cp -u mosaic.html /var/www/html
	cp -u mosaic.css  /var/www/css
	cp -u mosaic.js   /var/www/js
