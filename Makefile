UNAME := $(shell uname)

ifeq ($(UNAME), SunOS)
	slib = /home/courses/cse533/Stevens/unpv13e_solaris2.10
	LIBS =  -lsocket -lnsl lib/mylib.a ${slib}/libunp.a
else
	slib = ../unpv13e
	LIBS =  lib/mylib.a ${slib}/libunp.a
	UBUNTUF=-DUBUNTU
endif

CC = gcc


FLAGS =  -g -O2
CFLAGS = ${FLAGS} -I${slib}/lib

all: libmake client server

libmake: 
	${MAKE} -C lib

client: client.o lib/mylib.a lib/common.h
	${CC} ${FLAGS} -o client client.o lib/mylib.a ${LIBS}
client.o: client.c 
	${CC} ${CFLAGS} ${UBUNTUF} -c client.c

server: server.o lib/mylib.a lib/common.h
	${CC} ${FLAGS} -o server server.o lib/mylib.a ${LIBS}
server.o: server.c 
	${CC} ${CFLAGS} ${UBUNTUF} -c server.c

clean:
	@rm client server *.o ||:
	${MAKE} -C lib clean

