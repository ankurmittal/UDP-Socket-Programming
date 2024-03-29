#include "unprttplus.h"
#include "common.h"
#include "sendmessages.h"
#include <setjmp.h>
#define RTT_DEBUG
static struct rtt_info rttinfo;
static int rttinit = 0, cwin = 1, sst = 0, sw = 0, awindow, packintransit = 0, secondaryfd = 0, fd = 0;
static uint64_t sequence = 0;
static int head = -1, tail = -1, current = -1, csize = 0;
static struct msghdr *msgsend =  NULL;
static struct msghdr msgrecv; /* assumed init to 0 */
static void sig_alrm(int signo);
static sigjmp_buf jmpbuf;


void init_sender(int window, int f, int padvwindow) 
{
	int i;
	sw = sst = window;
	msgsend = (struct msghdr *) zalloc(sw * sizeof(struct msghdr));
	for(i =0; i < sw; i++)
	{
		msgsend[i].msg_iov = (struct iovec *) malloc(2 * sizeof(struct iovec));
		msgsend[i].msg_iov[0].iov_base = zalloc(sizeof(struct hdr));
		msgsend[i].msg_iov[0].iov_len = sizeof(struct hdr);
		msgsend[i].msg_iovlen = 2;
	}
	fd = f;
	sst = awindow = padvwindow;
}

void setsecondaryfd(int s)
{
	secondaryfd = s;
}

void setprimaryfd(int s)
{
	fd = s;
}

int isswfull()
{
	return csize == sw;
}
void insertmsg(void *outbuff, int outlen)
{
	if(head == -1)
		head = tail = 0;
	else 
		tail = (tail + 1)%sw;
	struct msghdr *mh = &msgsend[tail];
	struct hdr * hd = (struct hdr *)(mh->msg_iov[0].iov_base);
	hd->seq = sequence++;
	mh->msg_iov[1].iov_base = zalloc(outlen);
	memcpy(mh->msg_iov[1].iov_base, outbuff, outlen);
	mh->msg_iov[1].iov_len = outlen;
	csize++;
}

static void deletefromsw()
{
	if(!csize)
		return;
	csize--;
	struct msghdr *m = &msgsend[head];
	free(m->msg_iov[1].iov_base);
	if(head == tail)
		head = tail = current = -1;
	else
		head = (head + 1)%sw;
	return;
}

static uint64_t getseq(struct msghdr *mh)
{
	struct hdr * hd = (struct hdr *)mh->msg_iov[0].iov_base;
	return hd->seq;
}

static void printwindowcontent(int printnewline)
{
	int i;
	printf("{");
	for(i = 0; i < csize; i++)
	{
		printf("%" PRIu64 ", ", getseq(&msgsend[(head + i)%sw]));
	}
	printf("}");
	if(printnewline)
		printf("\n");
}


static struct hdr *gethdr(struct msghdr *mh)
{
	return (struct hdr *)mh->msg_iov[0].iov_base;
}

int dg_send(callback c)
{
	int window, cincr = 0;
	uint64_t lastseq = -1;
	int usesecondaryfd = 0, dupcount = 0, i, n, resettimer = 1;
	struct iovec iovrecv[1];
	struct msghdr *m;
	struct hdr recvhdr, *h;
	struct sigaction sa;
	struct itimerval timer;
	int hasmorepackets = 1;
	long stimer = 0, rto;
	char *congestioninfomsg;
	memset (&sa, 0, sizeof (sa));
	sa.sa_handler = &sig_alrm;
	if (rttinit == 0) {
		rtt_init_plus(&rttinfo); /* first time we're called */
		rttinit = 1;
		rtt_d_flag = 1;
	}
	msgrecv.msg_name = NULL;
	msgrecv.msg_namelen = 0;
	msgrecv.msg_iov = iovrecv;
	msgrecv.msg_iovlen = 1;
	bzero(&recvhdr, sizeof(struct hdr));
	iovrecv[0].iov_base = (char *)&recvhdr;
	iovrecv[0].iov_len = sizeof(struct hdr);
	sigaction(SIGALRM, &sa, NULL);
	rtt_newpack_plus(&rttinfo); /* initialize for this packet */
sendagain:
	window = min(cwin - packintransit, sw);
	window = min(awindow - packintransit, window);
	window = min(window, csize - packintransit);
	rto = rtt_start_plus(&rttinfo);
	if(cwin < sst)
		congestioninfomsg = "SLOW START";
	else
		congestioninfomsg = "CONGESTION AVOIDANCE";
	printf("\ncwin = %d, sst= %d, %s, adv window=%d, packet in transit=%d, window content=", cwin, sst, congestioninfomsg, awindow, packintransit);
	printwindowcontent(1);
	rtt_debug(&rttinfo);
	//printf("send window %d, pt: %d, ", window, packintransit);
	//printf("current:%d, head:%d, tail:%d", current, head, tail);
	for(i = 0; i < window; i++)
	{
		if(current == tail && csize < sw)
			break;
		if(!i)
			printf("Sending segment(s)");
		current = (current + 1)%sw;
		m = &msgsend[current];
		h = gethdr(m);
		printf (" %" PRIu64 ", ", h->seq);
		//h->ts = rtt_ts_plus(&rttinfo);
		n = sendmsg(fd, m, 0);
		packintransit++;
		if(usesecondaryfd && secondaryfd)
			n = sendmsg(secondaryfd, m, 0);
		if(n < 0 && !usesecondaryfd)
		{
			perror(" Error in sending data");
			return -1;
 		}
		if(n < 512 && h->seq)
			printf(" Last packet sent.");
	}
	if(resettimer) {
		stimer = rtt_ts_plus(&rttinfo);
		resettimer = 0;
	}
	if(window)
		printf("\n");
	usesecondaryfd = 0;
	
	setitimerwrapper(&timer, rto);
	if (sigsetjmp(jmpbuf, 1) != 0) {
		if (rtt_timeout_plus(&rttinfo) < 0) {
			err_msg("No response from client, giving up");
			rttinit = 0; /* reinit in case we're called again */
			errno = ETIMEDOUT;
			return (-1);
		}
		current = (head + sw - 1)%sw;
		packintransit = 0;
		if(awindow == 0)
		{
			printf("Adv Window was 0, probing client\n");
			awindow = 1;
			goto sendagain;
		}
		printf("Timeout, resending");
		printf(", timeout at: %uus\n", rtt_ts_plus(&rttinfo));
		sst = min(cwin/2, sw);
		sst = max(sst, 2);
		cwin = 1;
		cincr = 0;
		usesecondaryfd = 1;
		goto sendagain;
	}
recieveagain:
	h = gethdr(&msgsend[head]);
	if(secondaryfd) 
	{
		do {
			n = recvmsg(secondaryfd, &msgrecv, 0);
		} while(n < 0);
	}
	else
		n = recvmsg(fd, &msgrecv, 0);
	if(n < 0 && errno !=EINTR)
	{
		perror("Fatal Error while reading from client");
		close(fd);
		return -1;
	} 
	else if(n < 0 && errno == EINTR)
	{
		goto recieveagain;
	}
	if(recvhdr.seq < h->seq) {
		printf("Recieved old ack:%" PRIu64 ", discarding\n", recvhdr.seq);
		goto recieveagain;
	}
	if(recvhdr.seq == h->seq + 1 || recvhdr.seq == h->seq) {
		rtt_stop_plus(&rttinfo, rtt_ts_plus(&rttinfo) - stimer);
		resettimer = 1;
		printf("Resetting rtt timer.\n");
	}
	if(recvhdr.seq == lastseq + 1)
	{
		dupcount++;
		printf("Recieved dup ack:%" PRIu64 ", dup count:%d\n", recvhdr.seq, dupcount); 
	}
	else {
		packintransit = max(0, packintransit - (int)(recvhdr.seq - h->seq));
		printf("Recieved ack:%" PRIu64 "\n", recvhdr.seq); 
		lastseq = recvhdr.seq - 1;
		dupcount = 0;
	}
	if(dupcount > 2)
	{
		setitimerwrapper(&timer, 0);
		m = &msgsend[head];
		h = gethdr(m);
		//h->ts = rtt_ts_plus(&rttinfo);
		printf("Sending segment %" PRIu64 " again(Fast retransmission)\n", recvhdr.seq);
		if(cwin > sst) {
			cwin = sst;
			printf("Fast recovery\n");
		}
		n = sendmsg(fd, m, 0);
		dupcount = 0;
		setitimerwrapper(&timer, rtt_start_plus(&rttinfo));
		goto recieveagain;
	}

	setitimerwrapper(&timer, 0);
	if(cwin >= sst)
	{
		cincr += (int)(recvhdr.seq - h->seq);
		if(cincr >= cwin) {
			cwin++;
			cincr = cincr - cwin + 1;
		}
	}
	else
	{
		cincr = (int)(recvhdr.seq - h->seq);
		if(cwin + cincr > sst)
			cwin = sst;
		else
			cwin = cwin + cincr;
		cincr = 0;
	}
	awindow = recvhdr.window_size;
	for(i = 0; i < recvhdr.seq - h->seq; i++)
	{
		deletefromsw();
	}
	if(hasmorepackets == 0 && csize == 0)
		return 0;
	if(hasmorepackets && (sw - csize))
		hasmorepackets = c(sw - csize); 
	if(csize && (((current + 1)%sw < head && (current + 1)%sw + sw >= head + csize) 
			|| (current + 1)%sw >= head + csize))
	{
		//printf("resetting current:%d, %d, %d\n", current, head, csize);
		current = (head + sw - 1)%sw;
 	}
	rtt_newpack_plus(&rttinfo);/* initialize for this packet */
	goto sendagain;
}


static void sig_alrm(int signo)
{
	siglongjmp(jmpbuf, 1);
}
