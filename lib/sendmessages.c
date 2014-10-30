#include "unprttplus.h"
#include "common.h"
#include <setjmp.h>
#define RTT_DEBUG
static struct rtt_info rttinfo;
static int rttinit = 0, cwn = 1, sst = 0, sw = 0, rw = 1, packintransit = 0, lastseq = 0, secondaryfd = 0;
static uint32_t sequence = 0;
static int head = -1, tail = -1, current = -1, csize = 0;
static struct msghdr *msgsend =  NULL;
static struct msghdr msgrecv; /* assumed init to 0 */
struct hdr *sendhdr, *recvhdr;
static void sig_alrm(int signo);
static sigjmp_buf jmpbuf;

void init_sender(int window) 
{
	int i;
	sw = sst = window;
	msgsend = (struct msghdr *) malloc(sw * sizeof(struct msghdr));
	for(i =0; i < sw; i++)
	{
		msgsend[i].msg_iov = (struct iovec *) malloc(2 * sizeof(struct iovec));
		msgsend[i].msg_iov[0].iov_base = (struct hdr *) malloc(sizeof(struct hdr));
		msgsend[i].msg_iov[0].iov_len = sizeof(struct hdr);
		msgsend[i].msg_iovlen = 2;
	}
}

void setsecondaryfd(int s)
{
	secondaryfd = s;
}

static void insertmsg(void *outbuff, int outlen)
{
	if(head == -1)
		head = tail = 0;
	else 
		tail = (tail + 1)%sw;
	struct msghdr *mh = &msgsend[head];
	struct hdr * hd = (struct hdr *)mh->msg_iov[0].iov_base;
	hd->seq = sequence++;
	mh->msg_iov[1].iov_base = malloc(outlen);
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


static uint32_t getseq(struct msghdr *mh)
{
	struct hdr * hd = (struct hdr *)mh->msg_iov[0].iov_base;
	return hd->seq;
}

static struct hdr *gethdr(struct msghdr *mh)
{
	return (struct hdr *)mh->msg_iov[0].iov_base;
}

int dg_send(int fd)
{
	ssize_t n, window, awindow = 1;
	uint32_t startseq, lastseq = -1;
	int usesecondaryfd = 0, dupcount, i;
	struct iovec iovrecv[1];
	struct msghdr *m;
	struct hdr *h, recvhdr;
	if (rttinit == 0) {
		rtt_init(&rttinfo); /* first time we're called */
		rttinit = 1;
		rtt_d_flag = 1;
	}
	msgrecv.msg_name = NULL;
	msgrecv.msg_namelen = 0;
	msgrecv.msg_iov = iovrecv;
	msgrecv.msg_iovlen = 2;
	iovrecv[0].iov_base = &recvhdr;
	iovrecv[0].iov_len = sizeof(struct hdr);
	signal(SIGALRM, sig_alrm);
	rtt_newpack(&rttinfo); /* initialize for this packet */
sendagain:
	window = min(cwn - packintransit, sw);
	window = min(awindow, window);
	window = min(window, csize);
	for(i = 0; i < window; i++)
	{
		if(tail == -1)
			break;
		current = (current + 1)%sw;
		m = &msgsend[current];
		h = gethdr(m);
		if(i == 0)
			startseq = h->seq;
		h->ts = rtt_tt(&rttinfo);
		n = sendmsg(fd, m, 0);
		if(usesecondaryfd && secondaryfd)
			n = sendmsg(secondaryfd, m, 0);
		packintransit++;
		printf("%lu\n", n);
	}
	usesecondaryfd = 0;

	alarm(rtt_start(&rttinfo)); /* calc timeout value & start timer */
	if (sigsetjmp(jmpbuf, 1) != 0) {
		if (rtt_timeout(&rttinfo) < 0) {
			err_msg("No response from client, giving up");
			rttinit = 0; /* reinit in case we're called again */
			errno = ETIMEDOUT;
			return (-1);
		}
		sst = min(cwn/2, sw);
		sst = max(sst, 2);
		cwn = 1;
		usesecondaryfd = 1;
		current = (head + sw - 1)%sw;
		goto sendagain;
	}
	do {
		n = recvmsg(fd, &msgrecv, 0);
		if(n < 0)
		{
			perror("Error while reading from client");
			return -1;
		}
		if(recvhdr.seq == lastseq + 1)
			dupcount++;
		else {
			lastseq = recvhdr.seq - 1;
			dupcount = 0;
		}
		if(dupcount == 3)
		{
			
			m = &msgsend[head];
			h = gethdr(m);
			h->ts = rtt_tt(&rttinfo);
			n = sendmsg(fd, m, 0);
			dupcount == 0;
			alarm(rtt_start(&rttinfo)); /* calc timeout value & start timer */
		}
	} while (recvhdr.seq < startseq);
	alarm(0); /* stop SIGALRM timer */
	/* calculate & store new RTT estimator values */
	rtt_stop(&rttinfo, rtt_ts(&rttinfo) - recvhdr.ts);
	h = gethdr(&msgsend[head]);
	cwn = cwn + recvhdr.seq - h->seq;
	awindow = h->window_size;
	for(i = 0; i < recvhdr.seq - h->seq; i++)
	{
		deletefromsw();
	}
	goto sendagain;
}

static void sig_alrm(int signo)
{
	siglongjmp(jmpbuf, 1);
}
