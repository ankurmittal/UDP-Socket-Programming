#ifndef	__unp_rtt_h
#define	__unp_rtt_h

#include	"unp.h"

struct rtt_info {
  long		rtt_rtt;	/* most recent measured RTT, in milli seconds */
  long		rtt_srtt;	/* smoothed RTT estimator, in milli seconds */
  long		rtt_rttvar;	/* smoothed mean deviation, in milli seconds */
  long		rtt_rto;	/* current RTO to use, in seconds */
  int		rtt_nrexmt;	/* # times retransmitted: 0, 1, 2, ... */
  uint64_t		rtt_base;	/* # microsec since 1/1/1970 at start */
};

#define	RTT_RXTMIN     1000000	/* min retransmit timeout value, in seconds */
#define	RTT_RXTMAX     3000000	/* max retransmit timeout value, in seconds */
#define	RTT_MAXNREXMT 	12	/* max # times to retransmit */

				/* function prototypes */
void	 rtt_debug(struct rtt_info *);
void	 rtt_init_plus(struct rtt_info *);
void	 rtt_newpack_plus(struct rtt_info *);
int	     rtt_start_plus(struct rtt_info *);
void	 rtt_stop_plus(struct rtt_info *, uint32_t);
int		 rtt_timeout_plus(struct rtt_info *);
uint32_t rtt_ts_plus(struct rtt_info *);

extern int	rtt_d_flag;	/* can be set to nonzero for addl info */

#endif	/* __unp_rtt_h */
