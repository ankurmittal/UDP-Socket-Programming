#include "lib/unpifiplus.h"
#include "limits.h"
#include "lib/common.h"
#include "unprtt.h"
#include <setjmp.h>
#include <pthread.h>
#include <math.h>
#include <stdio.h>

#define TIMEOUTLIMIT 10

static struct SW {
	char *pkt;
	int length;
	uint64_t seq;
	pthread_spinlock_t lock;
}*sliding_window;

static const uint32_t localaddr = (127 << 24) + 1;
static uint64_t lastSeq = 0, headSeq = 0, firstSeq = 0, connected = 0;
static int lastAdvWindow = 0, timeoutcounter = 0;
static int ssize, sockfd, rseed, n, mean, finish = 0;
static int issamehost = 0, islocal = 0, dontroute = 1;
static float prob;
static struct timeval selectTime;
static struct msghdr msgsend, msgrecv;
static struct hdr sendhdr, recvhdr;
static struct iovec iovsend[1], iovrecv[2];
static struct sockaddr_in servaddr, taddr;
static void *inbuff;
static socklen_t len;
static pthread_t consumer;
static int timeout = 0;
static int finishConsumer = 0;
static int shouldDrop(int type, uint64_t seqNo) {
	double randomNo;
	randomNo = drand48();
	//printdebuginfo("randomNo: %lf, prob: %f\n", randomNo, prob);
	if(randomNo < prob) {
		if(type) {
			if (seqNo)
				printdebuginfo("Dropping Ack: %" PRIu64 "\n", seqNo);
			else
				printdebuginfo("Dropping First Connecting Packet.\n");
		}
		else
			printdebuginfo("Dropping Packet: %" PRIu64 "\n", seqNo);
		return 1;
	}
	else
		return 0;
}

static void printSlidingWindow2() {
	int i=0;
	printdebuginfo("SW2: {");
	for(i = (firstSeq%ssize); i <= (headSeq%ssize); i++) {
		if(sliding_window[i].pkt != NULL) {
			printdebuginfo("%" PRIu64 ",", sliding_window[i].seq);
		} else {
			printdebuginfo("-1,");
		}
	}
	printdebuginfo(" }\n");
}

static int randomGen() {
	double number = 0;
	double result = 0;

	number = drand48();
	result = log((double)number) * mean * -1;
	result *= 1000;
	//printdebuginfo("Sleeping for %d MicroSec.\n", (int)result);
	return (int)result;
}

int sendAck() {
	int n;
	sendhdr.seq = lastSeq;

	if(shouldDrop(1, lastSeq)) {
		return 1;
	}
	sendhdr.window_size = ssize - (int)(headSeq  - firstSeq) - 1;
	lastAdvWindow = ssize - (int)(headSeq  - firstSeq) - 1;
	printdebuginfo("Sending Ack= %" PRIu64 ", window_size= %d\n", sendhdr.seq, sendhdr.window_size);
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
			printdebuginfo("\n spinlock init failed\n");
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
	int thisConsumed = 0;
	char *data;
	int length = 0;
	uint64_t tempSeq, startSeq, endSeq;

	printdebuginfo("in consume\n");
	while(!finishConsumer) {
		startSeq = firstSeq;
		while(!thisConsumed) {
			tempSeq = firstSeq % ssize;
			//printdebuginfo("taking lock, tempSeq: %u\n", tempSeq);	
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
			//printdebuginfo("unlocking, tempSeq: %u\n", tempSeq);	

			if(data != NULL) {
				if(firstSeq != 1) {
			//		n = write(STDOUT_FILENO, data, length);
				}
				free(data);
			}
			if(firstSeq == lastSeq && finish) {
				printdebuginfo("Consumed Seq %" PRIu64 " - %" PRIu64 "\n", startSeq, firstSeq);
				return NULL;
			}

			if(lastAdvWindow == 0)
				sendAck();
		}

		endSeq = firstSeq;
		if(startSeq != endSeq)
			printdebuginfo("Consumed Sequence %" PRIu64 " : %" PRIu64 "\n", startSeq, endSeq-1);
		printSlidingWindow2();
		usleep(randomGen());
		thisConsumed = 0;
	}
	return NULL;
}

static void declareMsgHdr()
{
	inbuff = zalloc(datalength * sizeof(char));

	msgsend.msg_iov = iovsend;
	msgsend.msg_iovlen = 1;

	iovsend[0].iov_base = (char *)&sendhdr;
	iovsend[0].iov_len = sizeof(struct hdr);

	msgrecv.msg_iov = iovrecv;
	msgrecv.msg_iovlen = 2;

	iovrecv[0].iov_base = (char *)&recvhdr;
	iovrecv[0].iov_len = sizeof(struct hdr);
	iovrecv[1].iov_base = inbuff;
	iovrecv[1].iov_len = datalength;
}

static void insertToSw(ssize_t num) {
	uint64_t newSeq = recvhdr.seq;
	int length = num - sizeof(struct hdr);
	char *temp;

	if(headSeq + lastAdvWindow < newSeq)
		return;

	if(sliding_window[newSeq%ssize].pkt != NULL)
		return;

	temp = (char *) zalloc (datalength);
	memcpy(temp, inbuff, length);

	//printdebuginfo("X locking, newseq: %u\n", newSeq);
	pthread_spin_lock(&(sliding_window[newSeq % ssize].lock));
	sliding_window[newSeq%ssize].pkt = temp;
	sliding_window[newSeq%ssize].length = length;
	sliding_window[newSeq%ssize].seq = newSeq;
	pthread_spin_unlock(&(sliding_window[newSeq % ssize].lock));
	//printdebuginfo("X unlocking, newseq: %u\n", newSeq);

	if(newSeq == lastSeq)
	{
		lastSeq++;
		while(sliding_window[(lastSeq)%ssize].pkt != NULL 
				&& sliding_window[(lastSeq)%ssize].seq == lastSeq)
			lastSeq++;
	}

	if(newSeq > headSeq)
		headSeq = newSeq;	

	if(num<SEGLENGTH && connected) 
		finish = 1;

	printSlidingWindow2();
}

void dg_recv_send(int sockfd)
{
	fd_set allset;
	ssize_t num = 0;
	uint64_t newSeq;
	do {
		printdebuginfo("Recieving..!!\n");
		do {
			FD_ZERO(&allset);
			FD_SET(sockfd, &allset);
			select(sockfd+1, &allset, NULL, NULL, &selectTime);
			if(FD_ISSET(sockfd, &allset)) {
				num = recvmsg(sockfd, &msgrecv, 0);
				newSeq = recvhdr.seq;
				printdebuginfo("Recieve Seq: %" PRIu64 "\n", newSeq);
				timeoutcounter = 0;
			} else {
				timeout = 1;
				return;
			}

		} while(shouldDrop(0, recvhdr.seq));

	} while(newSeq < lastSeq && sendAck());

	if(newSeq)
		insertToSw(num);

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
		printdebuginfo("Server Address: %s\n", Sock_ntop((SA *) &servaddr, len));
		connected = 1;

		lastSeq = 1;
		firstSeq = 1;

		printdebuginfo("Creating new thread\n");
		pthread_create(&consumer, NULL, consume, NULL);
	}

	printdebuginfo("tail_seq: %" PRIu64 ", firstseq: %" PRIu64 "\n", headSeq, firstSeq);
	sendAck();

	return;
}

void resolveips(struct sockaddr_in *servaddr, struct sockaddr_in *cliaddr, uint32_t serverip)
{
	struct ifi_info *ifi, *ifihead;
	uint32_t clientip, pnetmask = 0, netmask;
	struct sockaddr_in *sa;
	for (ifihead = ifi = Get_ifi_info_plus(AF_INET, 1);
			ifi != NULL; ifi = ifi->ifi_next) {
		printdebuginfo(" IP addr: %s, ", Sock_ntop_host(ifi->ifi_addr, sizeof(*ifi->ifi_addr)));
		printdebuginfo("Network Mask: %s\n", Sock_ntop_host(ifi->ifi_ntmaddr, sizeof(*ifi->ifi_ntmaddr)));

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
		printdebuginfo(" Server is on same host, ");
		servaddr->sin_addr.s_addr = cliaddr->sin_addr.s_addr = ntohl(localaddr);
	}

	else if(islocal) {
		printdebuginfo(" Server is local, ");
	} else {
		dontroute = 0;
		printdebuginfo(" Server is not local, ");
		sa = (struct sockaddr_in *) ifihead->ifi_addr;
		cliaddr->sin_addr.s_addr = sa->sin_addr.s_addr;
	}
	printdebuginfo("IPserver: %s, ", inet_ntoa(servaddr->sin_addr));
	printdebuginfo("IPclient: %s\n", inet_ntoa(cliaddr->sin_addr));
	free_ifi_info_plus(ifihead);
}

int main(int argc, char **argv)
{
	int sport, reuse = 1, i=0;
	char serveripaddr[16], filename[PATH_MAX];
	struct sockaddr_in cliaddr;
	FILE *infile;
	char *ifile = "client.in";
	uint32_t serverip;
	time_t t1;
	(void) time(&t1);

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
		printdebuginfo("inet_pton error for %s\n", serveripaddr);
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
	ssize = lastAdvWindow = readint(infile);
	rseed = readint(infile);
	prob = readfloat(infile);
	mean = readint(infile);

	fclose(infile);

	//printdebuginfo("%s, %d, %s, %d, %d, %f, %d", serverip, sport, filename, ssize, rseed, prob, mean);

	sliding_window = (struct SW *)zalloc(ssize*sizeof(struct SW));

	srand48((long) t1 + rseed);

	printdebuginfo("initiating locks..!!\n");
	if(initiateLock() != 0) {
		//free all allocated memory and exit the program
	}
	printdebuginfo("locks initiated..!!\n");

	for(i=0; i<ssize; i++) {
		sliding_window[i].pkt = NULL;
		sliding_window[i].length = 0;
		sliding_window[i].seq = -1;
	}


	bzero(&cliaddr, sizeof(cliaddr));
	cliaddr.sin_family = AF_INET;
	resolveips(&servaddr, &cliaddr, serverip);
	cliaddr.sin_port = htons(0);

	printdebuginfo("In Main Thread..!!\n");

	sockfd = Socket (AF_INET, SOCK_DGRAM, 0);

	if(dontroute)
		setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR | SO_DONTROUTE, &reuse, sizeof(reuse));
	else
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

	printdebuginfo("Client Address: %s\n", Sock_ntop((SA *) &cliaddr, len));
	printdebuginfo("Server Address: %s\n", Sock_ntop((SA *) &servaddr, len));

sendagain:
	if(!shouldDrop(1, 0)) {
		printdebuginfo("writing first packet now..\n");
		n = write(sockfd, filename, strlen(filename));
		if (n<0) {
			perror("ERROR while writing data..!!\n");
			exit(1);
		}
	}

	declareMsgHdr();

	selectTime.tv_sec = 3;
	selectTime.tv_usec = 0;

	dg_recv_send(sockfd);
	if(timeout) {
		printdebuginfo("timeout first pkt\n");
		if(timeoutcounter < TIMEOUTLIMIT) {
			timeoutcounter++;
			timeout = 0;
			goto sendagain;	
		} else {
			printdebuginfo("Timeout Limit reached..!! Exiting now..!! \n");
			exit(1);
		}
	}

	while(!finish || lastSeq <= headSeq) {
		timeout = 0;
		selectTime.tv_sec = 10;
		selectTime.tv_usec = 0;
		dg_recv_send(sockfd);
		if (timeout) {
			printdebuginfo("timeout while recieving data..!!\n");
			finish = 1;
			finishConsumer = 1;
			break;
		}
	}

	if(!timeout) {
		printdebuginfo("client going in timewait state..!!\n");
		selectTime.tv_sec = 6;
		selectTime.tv_usec = 0;
		dg_recv_send(sockfd);
	}

	printdebuginfo("joining pthread..!!\n");
	pthread_join(consumer, NULL);
	destroyLock();

	close(sockfd);
	exit(0);
}
