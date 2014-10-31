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

SRC=$(wildcard *.c)
OBJS=$(SRC:.c=.o)


FLAGS =  -g -O2 -Wall
CFLAGS = ${FLAGS} -I${slib}/lib

all: ${OBJS} mylib.a prifinfo_plus

prifinfo_plus: prifinfo_plus.o get_ifi_info_plus.o
	${CC} -o prifinfo_plus prifinfo_plus.o get_ifi_info_plus.o ${LIBS}

mylib.a: get_ifi_info_plus.o rtt_plus.o linkedlist.o sendmessages.o common.o
	ar r mylib.a $^

%.o: %.c
	${CC} ${DEBUGFLAG} -c $^ $(CFLAGS)

clean:
	rm prifinfo_plus *.o mylib.a

