all: arcx


arcx: src/main.c obj/file.o
	gcc -I /usr/include src/main.c obj/file.o -o arcx

obj/file.o: src/file.h src/file.c
	gcc -c src/file.c -o obj/file.o