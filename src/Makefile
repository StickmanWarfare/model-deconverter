CC=gcc
LIBS=-lz
CFLAGS=

ifeq ($(PREFIX),)
	PREFIX := /usr/local
endif

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

stickmodel_decomp: main.o
	$(CC) -o $@ main.o $(LIBS) -static

clean :
	-rm *.o stickmodel_decomp
