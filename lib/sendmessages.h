
#ifndef	__sendmessages_h
#define	__sendmessages_h

typedef int (*callback)(int numberofsegments);
void init_sender(int window, int f, int awindow); 
void setsecondaryfd(int s);
void setprimaryfd(int s);
int isswfull();
void insertmsg(void *outbuff, int outlen);
int dg_send(callback c);
#endif	
