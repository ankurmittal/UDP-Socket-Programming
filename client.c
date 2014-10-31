#include "lib/unpifiplus.h"
#include "limits.h"
#include "lib/common.h"
#include "unprtt.h"
#include <setjmp.h>
#include <pthread.h>
#include <math.h>
#include <stdio.h>

static const uint32_t localaddr = (127 << 24) + 1;
static int issamehost = 0, islocal = 0;
static int rseed, mean;
static struct SW {
	char *pkt;
	int length;
	uint64_t seq;
	pthread_spinlock_t lock;
}*sliding_window;

static struct msghdr msgsend, msgrecv;
static struct hdr sendhdr, recvhdr;
static struct iovec iovsend[1], iovrecv[2];
static void *inbuff;
static struct sockaddr_in servaddr, taddr;
static socklen_t len;

static int ssize;
static uint64_t lastSeq = 0;
static uint64_t headSeq = 0;
static int connected = 0;
static uint64_t firstSeq = 0;
static int finish = 0;
static int lastAdvWindow = 0;
static int sockfd;

static void printSlidingWindow() {
	int i=0;
	printf("SW: {");
	for(i=0; i<ssize; i++) {
		if(sliding_window[i].pkt != NULL)
			printf("%" PRIu64 ",", sliding_window[i].seq);
		else
			printf("-1,");
	}
	printf(" }\n");
}

static int randomGen() {
	double number = 0;
	double result = 0;
	while(number == 0 || number == 1) {
		number = (double)rand()/(double)RAND_MAX;
	}
	result = log((double)number) * mean * -1;
	result *= 1000;
	printf("Sleeping for %d MicroSec.\n", (int)result);
	return (int)result;
}

int sendAck() {
	int n;
	sendhdr.seq = lastSeq;
	sendhdr.window_size = ssize - (int)(headSeq  - firstSeq) - 1;
	lastAdvWindow = ssize - (int)(headSeq  - firstSeq) - 1;
	printf("Sending Ack= %" PRIu64 ", window_size= %d\n", sendhdr.seq, sendhdr.window_size);
	n = sendmsg(sockfd, &msgsend, 0);
	if(n<0) {
		perror("Error sending ack, exiting..!!");
		exit(1);
	}
	return 1;
}

static int initiateLock() {
	int i = 0, locked = 0;	
	for(i=0; i<ssize; i++) {
		if (pthread_spin_init(&(sliding_window[i].lock), 0) != 0) {
			printf("\n spinlock init failed\n");
			locked = i;
			break;
		}
	}
	for(i=0; i<locked; i++) {
		pthread_spin_destroy(&(sliding_window[i].lock));
	}
	return locked;
}

static void destroyLock() {
	int i = 0;
	for (i=0; i<ssize; i++) {
		pthread_spin_destroy(&(sliding_window[i].lock));
	}
}

static void* consume() {
	int allConsumed = 0;
	int thisConsumed = 0;
	char *data;
	int length = 0;
	uint64_t tempSeq;
	srand(rseed);
	printf("in consume\n");
	while(!allConsumed) {
		while(!thisConsumed) {
			tempSeq = firstSeq % ssize;
			//printf("taking lock, tempSeq: %u\n", tempSeq);	
			pthread_spin_lock(&(sliding_window[tempSeq].lock));
			data = NULL;
			if (sliding_window[tempSeq].pkt != NULL) {
				length = sliding_window[tempSeq].length;
				data = sliding_window[tempSeq].pkt;
				sliding_window[tempSeq].pkt = NULL;
				firstSeq += 1;
			} else {
				thisConsumed = 1;
			}
			pthread_spin_unlock(&(sliding_window[tempSeq].lock));
			//printf("unlocking, tempSeq: %u\n", tempSeq);	
			
			if(data != NULL) {
				printf("printing content..!!\n");
				//write(0, data, length);
				free(data);
			}
			if(thisConsumed && finish)
				allConsumed = 1;

			if(lastAdvWindow == 0)
				sendAck();
			printSlidingWindow();
		}
		usleep(randomGen());
		thisConsumed = 0;
	}
}

static void declareMsgHdr(SA *destaddr, socklen_t destlen)
{
	inbuff = zalloc(datalength * sizeof(char));

	msgsend.msg_iov = iovsend;
	msgsend.msg_iovlen = 1;

	iovsend[0].iov_base = &sendhdr;
	iovsend[0].iov_len = sizeof(struct hdr);

	msgrecv.msg_iov = iovrecv;
	msgrecv.msg_iovlen = 2;

	iovrecv[0].iov_base = &recvhdr;
	iovrecv[0].iov_len = sizeof(struct hdr);
	iovrecv[1].iov_base = inbuff;
	iovrecv[1].iov_len = datalength;
}

static void insertToSw(ssize_t n) {
	uint64_t newSeq = recvhdr.seq;
	int length = n - sizeof(struct hdr);
	char *temp;

	if(newSeq-lastSeq >= ssize)
		return;
 	
	if(sliding_window[newSeq%ssize].pkt != NULL)
		return;

	temp = (char *) zalloc (datalength);
	memcpy(temp, inbuff, length);

	//printf("X locking, newseq: %u\n", newSeq);
 	pthread_spin_lock(&(sliding_window[newSeq % ssize].lock));
	sliding_window[newSeq%ssize].pkt = temp;
	sliding_window[newSeq%ssize].length = length;
	sliding_window[newSeq%ssize].seq = newSeq;
	pthread_spin_unlock(&(sliding_window[newSeq % ssize].lock));
	//printf("X unlocking, newseq: %u\n", newSeq);

	if(newSeq-lastSeq == 0)
		lastSeq++;

	if(newSeq > headSeq)
		headSeq = newSeq;	

	if(n<SEGLENGTH && connected) 
		finish = 1;
}

ssize_t dg_recv_send(int sockfd)
{
	ssize_t n;
	uint64_t newSeq;
	uint32_t ts;
	do {
		printSlidingWindow();
		printf("Recieving..!!\n");
		n = recvmsg(sockfd, &msgrecv, 0);
		newSeq = recvhdr.seq;
		//ts = recvhdr.ts;
		printf("Recieve Seq: %" PRIu64 "\n", newSeq);

	} while(newSeq < lastSeq && sendAck());

	insertToSw(n);
	if(!connected) {

		taddr.sin_family = AF_UNSPEC;
		len = sizeof(servaddr);
		servaddr.sin_port   = strtol(inbuff, NULL, 10);
		Connect(sockfd, (SA *) &servaddr, len);

		if (getpeername(sockfd, (SA *) &servaddr, &len) < 0) {
			perror("Error getting socket info for server.");
			close(sockfd);
			exit(1);
		}
		printf("Server Address: %s\n", Sock_ntop((SA *) &servaddr, len));
		connected = 1;
	}

	printf("tail_seq: %" PRIu64 "\n", headSeq);
	//sendhdr.ts = ts;
	sendAck();
	return (n-sizeof(struct hdr));
}

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
	if(issamehost) {
		printf(" Server is on same host, ");
		servaddr->sin_addr.s_addr = cliaddr->sin_addr.s_addr = ntohl(localaddr);
	}

	else if(islocal)
		printf(" Server is local, ");
	else 
	{
		printf(" Server is not local, ");
		sa = (struct sockaddr_in *) ifihead->ifi_addr;
		cliaddr->sin_addr.s_addr = sa->sin_addr.s_addr;
	}
	printf("IPserver: %s, ", inet_ntoa(servaddr->sin_addr));
	printf("IPclient: %s\n", inet_ntoa(cliaddr->sin_addr));
	free_ifi_info_plus(ifihead);
}

int main(int argc, char **argv)
{
	int sport, ret, servfd, reuse = 1, i=0;
	float prob;
	char serveripaddr[16], *temp, filename[PATH_MAX];
	struct sockaddr_in cliaddr;
	FILE *infile;
	char *ifile = "client.in";
	struct sockaddr_storage ss;
	uint32_t serverip;
	char buf[SEGLENGTH];
	struct iovec iovsend[2];
	int firstMessage = 1, locked = 0;
	pthread_t consumer;

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
	
	sliding_window = (struct SW *)zalloc(ssize*sizeof(struct SW));

	printf("initiating locks..!!\n");
	if(initiateLock() != 0) {
		//free all allocated memory and exit the program
	}
	printf("locks initiated..!!\n");
	
	for(i=0; i<ssize; i++) {
		sliding_window[i].pkt = NULL;
		sliding_window[i].length = 0;
		sliding_window[i].seq = -1;
	}

	printf("creating new thread\n");
	
	bzero(&cliaddr, sizeof(cliaddr));
	cliaddr.sin_family = AF_INET;
	resolveips(&servaddr, &cliaddr, serverip);
	cliaddr.sin_port = htons(0);

	printf("In Main Thread..!!\n");

	sockfd = Socket (AF_INET, SOCK_DGRAM, 0);
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

	len = sizeof(cliaddr);
	if(bind(sockfd, (SA *) &cliaddr, len) < 0) {
		perror("Not able to make local connection");
	}
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

	printf("Client Address: %s\n", Sock_ntop((SA *) &cliaddr, len));
	printf("Server Address: %s\n", Sock_ntop((SA *) &servaddr, len));

	write(sockfd, filename, strlen(filename));

	declareMsgHdr((SA *) &servaddr, sizeof(servaddr));
	
	pthread_create(&consumer, NULL, consume, NULL);
	
	while(!finish || lastSeq <= headSeq) {
		len = dg_recv_send(sockfd);
	}
	
	printf("joining pthread..!!\n");

	pthread_join(consumer, NULL);
	//destroyLock();
	
	close(sockfd);
}
