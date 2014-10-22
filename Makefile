
slib = ../unpv13e
#slib = /home/courses/cse533/Stevens/unpv13e_solaris2.10

CC = gcc

LIBS =  ${slib}/libunp.a

FLAGS =  -g -O2
CFLAGS = ${FLAGS} -I${slib}/lib

all: get_ifi_info_plus.o prifinfo_plus.o client
	${CC} -o prifinfo_plus prifinfo_plus.o get_ifi_info_plus.o ${LIBS}


get_ifi_info_plus.o: get_ifi_info_plus.c
	${CC} ${CFLAGS} -c get_ifi_info_plus.c

prifinfo_plus.o: prifinfo_plus.c
	${CC} ${CFLAGS} -c prifinfo_plus.c
	
client: client.o 
	${CC} ${FLAGS} -o client client.o  get_ifi_info_plus.o ${LIBS}
client.o: client.c 
	${CC} ${CFLAGS} -c client.c

clean:
	rm prifinfo_plus client *.o

