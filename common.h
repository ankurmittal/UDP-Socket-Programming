
#ifndef	__common_h
#define	__common_h

void *readstring(char *buf, int bufsize, FILE *f) 
{
	char *c;
	c = fgets(buf, bufsize, f);
	if(c == NULL)
	{
		perror("Error reading from file");
		fclose(f);
		exit(1);
	}
}

int readint(FILE *f) 
{
	char line[100];
	int temp;
	readstring(line, 100, f);

	temp = strtol(line, NULL, 10);
	if (errno) {
		perror("Error while converting from string");
		exit(1);
	}
	return temp;
}
#endif	
