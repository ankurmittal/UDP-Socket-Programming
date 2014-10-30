#include "lib/unpifiplus.h"
#include "lib/common.h"
#include "lib/linkedlist.h"

static const uint32_t localaddr = (127<<24) | 1;

typedef struct {
	int sockfd;
	struct in_addr *addr;
	struct in_addr *ntmaddr;
	struct in_addr *subaddr;
}SockStruct;

void handleChild(struct sockaddr_in *caddr, int cport, char *msg, int n, SockStruct *server) {

	struct sockaddr_in servaddr;
	socklen_t len;
	int port, sockfd;
	char buf[6];
	uint32_t clientip, serverip, servernetmask, serversubnet;

	printf("Client port: %d\n", cport);
	printf("Client IP Address: %s\n", inet_ntoa(caddr->sin_addr));
	printf("Data: %s, Size: %d\n", msg, n);

	clientip = htonl(caddr->sin_addr.s_addr);
	serverip = htonl(server->addr->s_addr);
	servernetmask = htonl(server->ntmaddr->s_addr);
	serversubnet = htonl(server->subaddr->s_addr);

	if(serverip == localaddr || serversubnet == clientip & servernetmask) {
		printf("client is local.\n");
		// DONTROUTE
	}

	sockfd = Socket(AF_INET, SOCK_DGRAM, 0);
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(0);

	len = sizeof(servaddr);
#ifndef UBUNTU
	printf("\nBIND\n");
	if(bind(sockfd, (SA *) &servaddr, len) < 0)
		perror("Not able to make local connection");
#endif
printf("\n cport: %d\n", cport);
	Connect(sockfd, (SA *)caddr, sizeof(*caddr));
	if (getsockname(sockfd, (SA *) &servaddr, &len) < 0) {
		perror("Error getting socket info for new socket.");
		close(sockfd);
		exit(1);
	}

	port = ntohs(servaddr.sin_port);
	sprintf(buf, "%d", port);
	
	//Write(server->sockfd, buf, strlen(buf));
	Sendto(server->sockfd, buf, strlen(buf), 0, (SA *)caddr, sizeof(*caddr));
}

int main(int argc, char **argv)
{
	int i=0, sockfd, maxfd=0, port=0, window=0, count=0;
	pid_t childpid, pid;
	const int on = 1;
	struct ifi_info *ifi, *ifihead;
	struct in_addr caddr;
	struct sockaddr_in *sa, ca;
	socklen_t len, clilen;
	char msg[MAXLINE];
	int n, cport, j;
	FILE *fp;
	SockStruct *array;
	struct in_addr subaddr;
	fd_set rset;

	fp = fopen("server.in", "r");
	if(fp == NULL)
	{
		perror("Error opening file - server.in");
		exit(1);
	}
	port = readint(fp);
	window = readint(fp);
	fclose(fp);

	for (ifihead = ifi = Get_ifi_info_plus(AF_INET, 1); ifi != NULL; ifi = ifi->ifi_next) {
		count++;	
	}

	array = (SockStruct *) malloc(count * sizeof(SockStruct));


	for (ifihead = ifi = Get_ifi_info_plus(AF_INET, 1), i=0; ifi != NULL; ifi = ifi->ifi_next, i++) {
		sockfd = Socket(AF_INET, SOCK_DGRAM, 0);
		Setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
		sa = (struct sockaddr_in *) ifi->ifi_addr;
		sa->sin_family = AF_INET;
		sa->sin_port = htons(port);
		Bind(sockfd, (SA *) sa, sizeof(*sa));
		array[i].addr = &((struct sockaddr_in *) ifi->ifi_addr)->sin_addr;
		array[i].ntmaddr = &((struct sockaddr_in *) ifi->ifi_ntmaddr)->sin_addr;
		subaddr.s_addr = (*array[i].addr).s_addr & (*array[i].ntmaddr).s_addr;
		array[i].subaddr = &subaddr;
		array[i].sockfd = sockfd;

		printf("\nPort: %d, Window: %d\n", port, window);
		printf("IP Address: %s\n", inet_ntoa(*(array[i].addr)));
		printf("Network Mask: %s\n", inet_ntoa(*(array[i].ntmaddr)));
		printf("Subnet Address: %s\n", inet_ntoa(*(array[i].subaddr)));

		listen(sockfd, LISTENQ);
	}

	for( ; ; ) {
		FD_ZERO(&rset);
		for(i=0; i<count; i++) {
			FD_SET(array[i].sockfd, &rset);
			maxfd = max(maxfd, array[i].sockfd);
		}
		Select(maxfd+1, &rset, NULL, NULL, NULL);

		for(i=0; i<count; i++) {
			if(FD_ISSET(array[i].sockfd, &rset)) {
				FD_CLR(array[i].sockfd, &rset);
				printf("\nIn select for sockfd: %d\n", array[i].sockfd);

				clilen = sizeof(ca);
				n = recvfrom(array[i].sockfd, msg, MAXLINE, 0, (SA *)&ca, &clilen);
				cport = ntohs(ca.sin_port);
				caddr = ca.sin_addr;
				if((childpid = fork()) == 0) {
					for(j = 0; j<count; j++) {
						if(j!=i)
							close(array[i].sockfd);
					}
					printf("IP Address: %s\n", inet_ntoa(*(array[i].addr)));
					printf("Network Mask: %s\n", inet_ntoa(*(array[i].ntmaddr)));
					printf("Subnet Address: %s\n", inet_ntoa(*(array[i].subaddr)));
					handleChild(&ca, cport, msg, n, &array[i]);
					goto exitLabel;
				}
			}
		}
	}
exitLabel:
	printf("\nExiting..!!\n");
	free_ifi_info_plus(ifihead);
	free(array);
	exit(0);
}
