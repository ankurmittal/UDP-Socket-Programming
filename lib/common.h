
#ifndef	__common_h
#define	__common_h

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include<sys/time.h>>

#define SEGLENGTH 512

struct hdr {
	uint32_t seq; /* sequence # */
	int window_size; /* window size*/
	uint32_t ts; /* timestamp when sent */
};

void *readstring(char *buf, int bufsize, FILE *f);

int readint(FILE *f);

float readfloat(FILE *f); 

static int datalength = SEGLENGTH - sizeof(struct hdr);

void *zalloc(size_t size);
void setitimerwrapper(struct itimerval *timer, long time);
#endif	
