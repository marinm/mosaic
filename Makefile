# Makefile

CCSTD=-std=c11

dev: mosaic.c
	gcc mosaic.c -DDEBUG -lgifenc -lexoquant -lfrozen -lpng -ljpeg -lm -g -o mosaic.cgi

commit: mosaic.html mosaic.css mosaic.js mosaic.cgi
	gcc mosaic.c -lgifenc -lexoquant -lfrozen -lpng -ljpeg -lm -g -o mosaic.cgi
	cp -u mosaic.cgi   /var/www/cgi
	cp -u mosaic.html  /var/www/html
	cp -u mosaic.css   /var/www/css
	cp -u mosaic.js    /var/www/js
