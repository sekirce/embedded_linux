CC=arm-linux-gnueabihf-gcc

CFLAGS=-g
LDFLAGS=-lsqlite3 -lpthread

OBJS=pds03_app.o

all:pds03_app

pds22_app:$(OBJS)
	${CC} ${CFLAGS} $? -o $@ ${LDFLAGS}

.PHONY: clean

clean:
	rm -f *.o
	rm -f pds03.app
	rm -f pds03_app


