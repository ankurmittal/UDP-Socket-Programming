
#ifndef	__common_h
#define	__common_h

#define SEGLENGHT 512

struct hdr {
	uint32_t seq; /* sequence # */
	int window_size; /* window size*/
	uint32_t ts; /* timestamp when sent */
};

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

int datalenght = SEGLENGHT - sizeof(struct hdr);

#endif	
