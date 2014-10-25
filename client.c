
#include "lib/unpifiplus.h"
#include "limits.h"
#include "lib/common.h"

static const uint32_t localaddr = (127 << 24) + 1;
static int issamehost = 0, islocal = 0;

void resolveips(struct sockaddr_in *servaddr, struct sockaddr_in *cliaddr, uint32_t serverip)
{
	struct ifi_info *ifi, *ifihead;
	uint32_t clientip, pnetmask = 0, netmask;
	struct sockaddr_in *sa;
	for (ifihead = ifi = Get_ifi_info_plus(AF_INET, 1);
			ifi != NULL; ifi = ifi->ifi_next) {
		printf(" IP addr: %s, ", Sock_ntop_host(ifi->ifi_addr, sizeof(*ifi->ifi_addr)));
		printf("Network Mask: %s\n", Sock_ntop_host(ifi->ifi_ntmaddr, sizeof(*ifi->ifi_ntmaddr)));

		if(!issamehost)
		{
			sa = (struct sockaddr_in *) ifi->ifi_addr;
			clientip = htonl(sa->sin_addr.s_addr);
			if(clientip == serverip)
			{
				issamehost = 1;
				servaddr->sin_addr.s_addr = cliaddr->sin_addr.s_addr = ntohl(localaddr);
				continue;
			}
			sa = (struct sockaddr_in *) ifi->ifi_ntmaddr;
			netmask = htonl(sa->sin_addr.s_addr);
			if(!islocal || netmask > pnetmask)
			{
				if( (clientip & netmask) == (serverip & netmask))
				{
					pnetmask = netmask;
					islocal = 1;
					sa = (struct sockaddr_in *) ifi->ifi_addr;
					cliaddr->sin_addr.s_addr = sa->sin_addr.s_addr;
				}

			}
		}

	}
	if(!issamehost && !islocal)
	{
		sa = (struct sockaddr_in *) ifihead->ifi_addr;
		cliaddr->sin_addr.s_addr = sa->sin_addr.s_addr;
	}
	if(issamehost)
		printf(" Server is on same host, ");
	else if(islocal)
		printf(" Server is local, ");
	else 
		printf(" Server is not local, ");
	printf("IPserver: %s, ", inet_ntoa(servaddr->sin_addr));
	printf("IPclient: %s\n", inet_ntoa(cliaddr->sin_addr));
	free_ifi_info_plus(ifihead);
}

int main(int argc, char **argv)
{
	int sockfd, sport, ret, ssize, rseed, mean, servfd;
	float prob;
	char serveripaddr[16], *temp, filename[PATH_MAX];
	struct sockaddr_in servaddr, cliaddr, taddr;
	FILE *infile;
	char *ifile = "client.in";
	struct sockaddr_storage ss;
	uint32_t serverip;
	char buf[512];
	socklen_t len;

	infile = fopen(ifile, "r");
	if(infile == NULL)
	{
		perror("Error opening file - client.in");
		exit(1);
	}
	readstring(serveripaddr, 16, infile);
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	if (inet_pton(AF_INET, serveripaddr, &servaddr.sin_addr) <= 0) 
	{
		printf("inet_pton error for %s\n", serveripaddr);
		fclose(infile);
		exit(1);
	}
	serverip = htonl(servaddr.sin_addr.s_addr);
	if(serverip == localaddr) {
		issamehost = 1;
	}
	sport = readint(infile);
	servaddr.sin_port   = htons(sport);
	readstring(filename, PATH_MAX, infile);
	ssize = readint(infile);
	rseed = readint(infile);
	prob = readfloat(infile);
	mean = readint(infile);

	fclose(infile);

	//printf("%s, %d, %s, %d, %d, %f, %d", serverip, sport, filename, ssize, rseed, prob, mean);

	bzero(&cliaddr, sizeof(cliaddr));
	cliaddr.sin_family = AF_INET;
	resolveips(&servaddr, &cliaddr, serverip);
	cliaddr.sin_port = htons(0);
	
	sockfd = Socket (AF_INET, SOCK_DGRAM, 0);

#ifndef UBUNTU
	len = sizeof(cliaddr);
	if(bind(sockfd, (SA *) &cliaddr, len) < 0) {
		perror("Not able to make local connection");
	}
#endif
	len = sizeof(servaddr);
	Connect(sockfd, (SA *) &servaddr, len);
	if (getpeername(sockfd, (SA *) &servaddr, &len) < 0)
	{
		perror("Error getting socket info for server.");
		close(sockfd);
		exit(1);
	}
	
	len = sizeof(cliaddr);
	if (getsockname(sockfd, (SA *) &cliaddr, &len) < 0)
	{
		perror("Error getting socket info for client.");
		close(sockfd);
		exit(1);
	}

	printf(" Client Address: %s\n", Sock_ntop((SA *) &cliaddr, len));
	printf(" Server Address: %s\n", Sock_ntop((SA *) &servaddr, len));

	write(sockfd, filename, strlen(filename));
	len = recvfrom(sockfd, buf, 512, 0, NULL, NULL);
	
	puts(buf);
	taddr.sin_family = AF_UNSPEC;

	servaddr.sin_port   = strtol(buf, NULL, 10);

	connect(sockfd, (SA *) &taddr, len);
	connect(sockfd, (SA *) &servaddr, len);
	if (getpeername(sockfd, (SA *) &servaddr, &len) < 0)
	{
		perror("Error getting socket info for server.");
		close(sockfd);
		exit(1);
	}
	printf(" Server Address: %s\n", Sock_ntop((SA *) &servaddr, len));
}

