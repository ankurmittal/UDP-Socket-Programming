#include "lib/unpifiplus.h"
#include "lib/common.h"
#include "lib/sendmessages.h"
#include "lib/linkedlist.h"

static const uint32_t localaddr = (127<<24) | 1;
static int window;
static FILE *filefd;

typedef struct {
	int sockfd;
	struct in_addr *addr;
	struct in_addr *ntmaddr;
	struct in_addr *subaddr;
}SockStruct;

typedef struct {
	int port;
	struct in_addr addr;
}NodeData;

int fillslidingwindow(int segments)
{

}

void handleChild(struct sockaddr_in *caddr, char *msg, SockStruct *server) {

	struct sockaddr_in servaddr;
	socklen_t len;
	int port, sockfd, reuse = 1, err, n;
	char buf[6];
	uint32_t clientip, serverip, servernetmask, serversubnet;
	len = sizeof(*caddr);
	printf(" Filename recieved from client: %s\n", msg);
	printf(" Client Address: %s\n", Sock_ntop((SA *) caddr, len));

	clientip = htonl(caddr->sin_addr.s_addr);
	serverip = htonl(server->addr->s_addr);
	servernetmask = htonl(server->ntmaddr->s_addr);
	serversubnet = htonl(server->subaddr->s_addr);

	if(serverip == localaddr || serversubnet == clientip & servernetmask) {
		printf(" Client is local.\n");
		// DONTROUTE
	}

	sockfd = Socket(AF_INET, SOCK_DGRAM, 0);
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(0);
	servaddr.sin_addr = *(server->addr);

	len = sizeof(servaddr);

	if(bind(sockfd, (SA *) &servaddr, len) < 0)
	{
		perror("Not able to make local connection");
		close(server->sockfd);
		return;
	}

	connect(sockfd, (SA *)caddr, sizeof(*caddr));
	connect(server->sockfd, (SA *)caddr, sizeof(*caddr));

	if (getpeername(sockfd, (SA *) &caddr, &len) < 0)
	{
		perror("Error getting socket info for server.");
		close(sockfd);
		close(server->sockfd);
		return;
	}
	
	if (getsockname(sockfd, (SA *) &servaddr, &len) < 0)
	{
		perror("Error getting socket info for client.");
		close(sockfd);
		close(server->sockfd);
		return;
	}
	printf(" Client Address: %s\n", Sock_ntop((SA *) &caddr, len));
	printf(" Server Address: %s\n", Sock_ntop((SA *) &servaddr, len));

	
	init_sender(window, server->sockfd);

	n = sprintf(buf, "%d", servaddr.sin_port);
	setsecondaryfd(sockfd);
	writetowindow(buf, n + 1);
	dg_send(fillslidingwindow);
	//close(&filefd);
	
}


int writetowindow(char * buf, int len)
{
	if(!isswfull())
		insertmsg(buf, len);
	else
		return -1;
}

int ll_find(ll_node *ll_head, NodeData *data) {
	NodeData *ll_data;
	if(ll_head == NULL)
		return 0;
	do {
		ll_data = (NodeData *)ll_head->data;
		if(ll_data->port == data->port && ll_data->addr.s_addr == data->addr.s_addr)
			return 1;
		ll_head = ll_head -> next;
	} while(ll_head != NULL);
	return 0;
}

void ll_print(ll_node *ll_head) {
	NodeData *ll_data;	
	if(ll_head == NULL)
		return;
	else {
		ll_data = (NodeData *)ll_head->data;
		printf("PORT: %d, ADDR: %s\n", ll_data->port, inet_ntoa(ll_data->addr));
		ll_head = ll_head->next;
	}
}

int main(int argc, char **argv)
{
	int i=0, sockfd, maxfd=0, port=0, count=0;
	pid_t childpid, pid;
	const int on = 1;
	struct ifi_info *ifi, *ifihead;
	struct sockaddr_in *sa, ca;
	socklen_t len, clilen;
	char msg[MAXLINE];
	int n, cport, j;
	FILE *fp;
	SockStruct *array;
	struct in_addr subaddr;
	fd_set rset;
	ll_node *ll_head = NULL;
	NodeData *ll_data;

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

	printf("\nPort: %d, Window: %d\n", port, window);
	array = (SockStruct *) malloc(count * sizeof(SockStruct));

	for (ifihead = ifi = Get_ifi_info_plus(AF_INET, 1), i=0; ifi != NULL; ifi = ifi->ifi_next, i++) {
		sockfd = Socket(AF_INET, SOCK_DGRAM, 0);
		sa = (struct sockaddr_in *) ifi->ifi_addr;
		sa->sin_family = AF_INET;
		sa->sin_port = htons(port);
		Bind(sockfd, (SA *) sa, sizeof(*sa));
		array[i].addr = &(sa->sin_addr);
		array[i].ntmaddr = &((struct sockaddr_in *) ifi->ifi_ntmaddr)->sin_addr;
		subaddr.s_addr = (*array[i].addr).s_addr & (*array[i].ntmaddr).s_addr;
		array[i].subaddr = &subaddr;
		array[i].sockfd = sockfd;

		printf("IP Address: %s\n", inet_ntoa(*(array[i].addr)));
		printf("Network Mask: %s\n", inet_ntoa(*(array[i].ntmaddr)));
		printf("Subnet Address: %s\n", inet_ntoa(*(array[i].subaddr)));

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
				printf("\nIn select for sockfd: %d\n", array[i].sockfd);

				clilen = sizeof(ca);
				n = recvfrom(array[i].sockfd, msg, MAXLINE, 0, (SA *)&ca, &clilen);
				if(n < 0)
				{
					break;
				}

				ll_data = (NodeData *) malloc(sizeof(NodeData));
				ll_data->port = ca.sin_port;
				ll_data->addr = ca.sin_addr;

				if (!ll_find(ll_head, ll_data)) {
					if(ll_head == NULL)
						ll_head = ll_initiate(ll_data);
					else
						ll_insert(ll_head, ll_data);
						
					ll_print(ll_head);

					if((childpid = fork()) == 0) {
						for(j = 0; j<count; j++) {
							if(j != i)
								close(array[i].sockfd);
						}
						printf("IP Address: %s\n", inet_ntoa(*(array[i].addr)));
						printf("Network Mask: %s\n", inet_ntoa(*(array[i].ntmaddr)));
						printf("Subnet Address: %s\n", inet_ntoa(*(array[i].subaddr)));
						handleChild(&ca, msg, &array[i]);
						goto exitLabel;
					}
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
