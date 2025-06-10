all: fat-sys

fat-sys: fat.o ds.o cmd.o 
	gcc -o fat-sys fat.o ds.o cmd.o -lm
	
fat.o: fat.h fat.c
	gcc fat.c -c -o fat.o

cmd.o: cmd.c
	gcc cmd.c -c -o cmd.o 

ds.o: ds.h ds.c
	gcc ds.c -c -o ds.o

dev: fat-sys
	./fat-sys imagem-pronta 20

img:
	dd if=/dev/zero of=nova-imagem count=20 bs=4k

ver:
	hexdump -C nova-imagem | less
