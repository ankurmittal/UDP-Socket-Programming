#include "unpifiplus.h"
#include "common.h"

typedef struct {
	int sockfd;
	struct in_addr *addr;
	struct in_addr *ntmaddr;
	struct in_addr *subaddr;
}SockStruct;

int main(int argc, char **argv)
{
	int i=0, sockfd, maxfd=0, port=0, window=0, count=0;
	const int on = 1;
	pid_t pid;
	struct ifi_info *ifi, *ifihead;
	struct sockaddr_in *sa;
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

	FD_ZERO(&rset);

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
	}

	free_ifi_info_plus(ifihead);
	for( ; ; ) {
		for(i=0; i<count; i++) {
			FD_SET(array[i].sockfd, &rset);
			maxfd = max(maxfd, array[i].sockfd);
		}
		Select(maxfd+1, &rset, NULL, NULL, NULL);
		
		for(i=0; i<count; i++) {
			if(FD_ISSET(array[i].sockfd, &rset)) {
				printf("\nIn select for sockfd: %d\n", array[i].sockfd);
				// Do Something
			}
		}
	}

	printf("\nExiting..!!\n");
	free(array);
	exit(0);
}
