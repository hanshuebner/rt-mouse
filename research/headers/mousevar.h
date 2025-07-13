/*
 * 5799-WZQ (C) COPYRIGHT IBM CORPORATION 1986,1987
 * LICENSED MATERIALS - PROPERTY OF IBM
 * REFER TO COPYRIGHT INSTRUCTIONS FORM NUMBER G120-2083
 */
/* $Header: /usr/src/sys/rtio/RCS/mousevar.h,v 1.1 1988/12/09 22:51:43 md Exp $ */
/* $ACIS:mousevar.h 12.0$ */
/* $Source: /usr/src/sys/rtio/RCS/mousevar.h,v $ */

#if !defined(lint) && !defined(LOCORE)  && defined(RCS_HDRS)
static char *rcsidmousevar = "$Header: /usr/src/sys/rtio/RCS/mousevar.h,v 1.1 1988/12/09 22:51:43 md Exp $";
#endif

/* mouse software structures and defines */
struct mouseq {
	struct mouseq	*qp_next;	/* pointer to the next mouse cmd */
	struct klsq	cmd1,cmd2;	/* kls commands */
	int		ms_count;
};

struct ms_softc {
	struct mouseq	*ms_head;	/* head of mouse cmd queue */
	struct mouseq	*ms_tail;	/* tail of mouse cmd queue */
	int		ms_sem;		/* semaphore protects some critical */
					/* sections of mouse code. */
	int		ms_flags;	/* mouse status flags */
	int		msenabled;	/* set if mouse is enabled for input */
	long		ms_rate;	/* last rate mouse set to (used for asyc */
	int		ms_cnt;		/* count of opened mouse streams */
					/* resets) */
	long		ms_resl;		/* last resolution mouse set to (used for */
	int		ms_sunit;
					/* async resets) */
};
#define MS_QUERY	0x2		/* query routine waiting for mouse data */
#define MS_BUSY	0x1		/* query routine using mouse data */
#define MS_NOSTREAM 0x4		/* use remote not streaming mode */
#define MS_EXP	0x8		/* use exponential mode */

#define MSPOOLSZ	10
#define MSPRI		(PZERO+5)

#define ISPEED B9600
