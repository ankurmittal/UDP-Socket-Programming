
#include "unpifiplus.h"
#include "limits.h"
#include "common.h"

int main(int argc, char **argv)
{
	int sockfd, sport, ret, ssize, rseed, mean;
	float prob;
	const int on = 1;
	pid_t pid;
	char serverip[16], *temp, filename[PATH_MAX];
	struct ifi_info *ifi, *ifihead;
	struct sockaddr_in *sa, cliaddr, wildaddr;
	FILE *infile;
	char *ifile = "client.in";
	infile = fopen(ifile, "r");
	if(infile == NULL)
	{
		perror("Error opening file - client.in");
		exit(1);
	}
	readstring(serverip, 16, infile);
	sport = readint(infile);
	readstring(filename, PATH_MAX, infile);
	ssize = readint(infile);
	rseed = readint(infile);
	prob = readfloat(infile);
	mean = readint(infile);

	printf("%s, %d, %s, %d, %d, %f, %d", serverip, sport, filename, ssize, rseed, prob, mean);
	exit(0);



	for (ifihead = ifi = Get_ifi_info_plus(AF_INET, 1);
			ifi != NULL; ifi = ifi->ifi_next) {
		/* bind unicast address */
		sockfd = Socket(AF_INET, SOCK_DGRAM, 0);
		Setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
		sa = (struct sockaddr_in *) ifi->ifi_addr;
		sa->sin_family = AF_INET;
		sa->sin_port = htons(SERV_PORT);
		Bind(sockfd, (SA *) sa, sizeof(*sa));
		printf("bound %s\n", Sock_ntop((SA *) sa, sizeof(*sa)));
		if ( (pid = Fork()) == 0) { /* child */
			//mydg_echo(sockfd, (SA *) &cliaddr, sizeof(cliaddr), (SA *)
			//		sa);
			exit(0); /* never executed */

		}
		if (ifi->ifi_flags & IFF_BROADCAST) {
			/* try to bind broadcast address */
			sockfd = Socket(AF_INET, SOCK_DGRAM, 0);
			Setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
			sa = (struct sockaddr_in *) ifi->ifi_brdaddr;
			sa->sin_family = AF_INET;
			sa->sin_port = htons(SERV_PORT);
			if (bind(sockfd, (SA *) sa, sizeof(*sa)) < 0) {
				if (errno == EADDRINUSE) {
					printf("EADDRINUSE: %s\n",
							Sock_ntop((SA *) sa, sizeof(*sa)));
					Close(sockfd);
					continue;
				} else
					err_sys("bind error for %s",
							Sock_ntop((SA *) sa, sizeof(*sa)));
			}
			printf("bound %s\n", Sock_ntop((SA *) sa, sizeof(*sa)));
			if ( (pid = Fork()) == 0) { /* child */
				//mydg_echo(sockfd, (SA *) &cliaddr, sizeof(cliaddr),
	//					(SA *) sa);
				exit(0); /* never executed */
			}
		}
	}
	/* bind wildcard address */
	sockfd = Socket(AF_INET, SOCK_DGRAM, 0);
	Setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
	bzero(&wildaddr, sizeof(wildaddr));
	wildaddr.sin_family = AF_INET;
	wildaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	wildaddr.sin_port = htons(SERV_PORT);
	Bind(sockfd, (SA *) &wildaddr, sizeof(wildaddr));
	printf("bound %s\n", Sock_ntop((SA *) &wildaddr,
				sizeof(wildaddr)));
	if ( (pid = Fork()) == 0) { /* child */
		//mydg_echo(sockfd, (SA *) &cliaddr, sizeof(cliaddr), (SA *) sa);
		exit(0); /* never executed */
	}
	exit(0);
}

