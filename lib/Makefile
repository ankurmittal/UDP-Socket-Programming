UNAME := $(shell uname)

ifeq ($(UNAME), SunOS)
	slib = /home/courses/cse533/Stevens/unpv13e_solaris2.10
	LIBS =  -lsocket -lnsl ${slib}/libunp.a
else
	slib = ../../unpv13e
	LIBS =  ${slib}/libunp.a
	UBUNTUF=-DUBUNTU
endif

CC = gcc


FLAGS =  -g -O2
CFLAGS = ${FLAGS} -I${slib}/lib

all: mylib.a prifinfo_plus

prifinfo_plus: prifinfo_plus.o get_ifi_info_plus.o
	${CC} -o prifinfo_plus prifinfo_plus.o get_ifi_info_plus.o ${LIBS}

get_ifi_info_plus.o: get_ifi_info_plus.c
	${CC} ${CFLAGS} -c get_ifi_info_plus.c

prifinfo_plus.o: prifinfo_plus.c
	${CC} ${CFLAGS} -c prifinfo_plus.c
	
rtt_plus.o: rtt_plus.c
	${CC} ${CFLAGS} -c rtt_plus.c

mylib.a: get_ifi_info_plus.o rtt_plus.o
	ar r mylib.a rtt_plus.o get_ifi_info_plus.o

clean:
	rm prifinfo_plus *.o mylib.a

