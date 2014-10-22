
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
	uint32_t clientip;
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

	//printf("%s, %d, %s, %d, %d, %f, %d", serverip, sport, filename, ssize, rseed, prob, mean);



	for (ifihead = ifi = Get_ifi_info_plus(AF_INET, 1);
			ifi != NULL; ifi = ifi->ifi_next) {
		sa = (struct sockaddr_in *) ifi->ifi_addr;
		printf("  IP addr: %s\n", Sock_ntop_host(ifi->ifi_addr, sizeof(*ifi->ifi_addr)));
		printf("  IP addr: %s\n", Sock_ntop_host(ifi->ifi_ntmaddr, sizeof(*ifi->ifi_ntmaddr)));
		clientip = htonl(sa->sin_addr.s_addr);

	}
}

