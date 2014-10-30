#include "common.h"
void *readstring(char *buf, int bufsize, FILE *f) 
{
	char *c;
	int s;
	c = fgets(buf, bufsize, f);
	if(c == NULL)
	{
		perror("Error reading from file");
		fclose(f);
		exit(1);
	}
	s = strlen(buf);
	if(buf[s-1] == '\n')
		buf[s - 1]  = 0;

}

int readint(FILE *f) 
{
	char line[100];
	int temp;
	readstring(line, 100, f);

	temp = strtol(line, NULL, 10);
	if (errno) {
		perror("Error while converting from string");
		fclose(f);
		exit(1);
	}
	return temp;
}

float readfloat(FILE *f) 
{
	char line[100];
	float temp;
	readstring(line, 100, f);

	temp = strtof(line, NULL);
	if (errno) {
		perror("Error while converting from string");
		fclose(f);
		exit(1);
	}
	return temp;
}

void *zalloc(size_t size)
{
	void *p = malloc(size);
	memset(p, 0, size);
	return p;
}


void setitimerwrapper(struct itimerval *timer, long time)
{
	timer->it_value.tv_sec = time / 1000;
	timer->it_value.tv_usec = (time%1000) * 1000;
	printf("Setting timer to:%ld\n",time);
	timer->it_interval.tv_sec = time / 1000;
	timer->it_interval.tv_usec = (time%1000) * 1000;
	setitimer (ITIMER_REAL, timer, NULL);
}
