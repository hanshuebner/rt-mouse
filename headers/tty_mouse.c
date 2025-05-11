/*
 * 5799-WZQ (C) COPYRIGHT IBM CORPORATION  1986,1987,1988
 * LICENSED MATERIALS - PROPERTY OF IBM
 * REFER TO COPYRIGHT INSTRUCTIONS FORM NUMBER G120-2083
 */
/* $Header: /sys/rt/dev/RCS/tty_mouse.c,v 1.7 1994/05/22 12:24:51 roger Exp $ */
/* $ACIS:tty_mouse.c 12.0$ */
/* $Source: /sys/rt/dev/RCS/tty_mouse.c,v $ */

#if !defined(lint) && !defined(NO_RCS_HDRS)
static char *rcsid = "$Header: /sys/rt/dev/RCS/tty_mouse.c,v 1.7 1994/05/22 12:24:51 roger Exp $";
#endif

/* tty_mouse.c -- mouse discipline routines */

#include "ms.h"
#if NMS > 0

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/user.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/conf.h>
#include <sys/buf.h>
#include <sys/uio.h>
#include "rt/include/mouseio.h"
#include "rt/dev/mousereg.h"

#ifdef IBMRTPC
#define	 MS_REPORT_SZ   sizeof(mousedata)
#endif IBMRTPC

#ifdef ATR
#define	 MS_REPORT_SZ   3
#endif ATR

struct mb {
	int	   used;
	int 	   single_but;
	int	   inbuf;		/* count of bytes in buffer */
	char	   *bufp; 		/* buffer pointer (to ms_report) */
	mousedata  ms_report; 
} mb[NMS];

/*
 * Open as mouse discipline.  Called when discipline changed
 * with ioctl, and changes the interpretation of the information
 * in the tty structure.
 */
/*ARGSUSED*/
msdopen(dev, tp)
	dev_t dev;
	register struct tty *tp;
{
	register struct mb *mbp;

	if (tp->t_line == MSLINEDISC)
		return (ENODEV);
	ttywflush(tp);
	for (mbp = mb; mbp < &mb[NMS]; mbp++)
		if (!mbp->used)
			break;
	if (mbp >= &mb[NMS])
		return (EBUSY);
	mbp->used++;
	mbp->single_but = 0;
	mbp->inbuf = 0;
	mbp->bufp = (char *) &mbp->ms_report;
	mbp->ms_report.m_magic = 0;
	mbp->ms_report.m_flags = 0;
	mbp->ms_report.m_x  = 0;
	mbp->ms_report.m_y  = 0;
	tp->t_rawq.c_cc = 0;
	tp->t_canq.c_cc = 0;
	tp->T_LINEP = (caddr_t) mbp;
	return (0);
}

/*
 * Break down... called when discipline changed or from device
 * close routine.
 */
msdclose(tp)
	register struct tty *tp;
{
	register int s = MS_SPL;

	((struct mb *) tp->T_LINEP)->used = 0;
	tp->t_rawq.c_cc = 0;		/* clear queues -- paranoid */
	tp->t_line = 0;			/* paranoid: avoid races */
	splx(s);
}

/*
 * Read from a mouse.
 * Characters have been buffered in the raw queue. 
 */
msdread(tp, uio)
	register struct tty *tp;
	struct uio *uio;
{
	int	s, error = 0;
	int	c;

	s = MS_SPL;
	while (tp->t_rawq.c_cc <= 0) {
		if (tp->t_state & TS_ASYNC) {
			splx(s);
			return (EWOULDBLOCK);
		}
		sleep((caddr_t)&tp->t_rawq, TTIPRI);
	}
	splx(s);

	/* Input present. */
	while ((c = getc(&tp->t_rawq)) >= 0) {
		error = ureadc(c, uio);		
		if (error || (uio->uio_resid == 0))
			break;
	}
	return (error);
}
	

/*
 * Low level character input routine.
 * Stuff the character in the buffer.
 *
 * This routine could be expanded in-line in the receiver
 * interrupt routine of the mouse adapter to make it run as fast as possible.
 * Logic:
 * 1. collect up mouse report characters with mbp->inbuf being the count
 *	of number of characters collected so far and mbp->bufp being the
 *	position to store the next character.
 * 2. when we have an entire report call msddecode.
 * 3. if msddecode indicates a we have a complete report we transfer from
 *	our buffer to the tty raw q and wakeup the tty.
 */
 
msdinput(c, tp)
	register int c;
	register struct tty *tp;
{
	register struct mb *mbp = (struct mb *) tp->T_LINEP;

#ifdef IBMRTPC
	if((c == MS_DATA_SYNC) || (mbp->inbuf > 0)) {
#endif IBMRTPC
#ifdef ATR
	if(((c & MS_SYNC_MASK) == MS_DATA_SYNC) || (mbp->inbuf > 0)) {
#endif ATR
		*mbp->bufp++ = c;
		if (++mbp->inbuf == MS_REPORT_SZ) {
			if (msddecode(mbp->inbuf, mbp, tp)) {
				msreport(mbp, tp);
			}
			mbp->bufp  = (char *) &mbp->ms_report;
			mbp->inbuf = 0;		/* prepare for next report */
		}
	}
}

/*
 * copy mouse data into raw q and wakeup the tty
 */
msreport(mbp, tp)
	register struct tty *tp;
	register struct mb *mbp;
{
	register int i;
	register char *p;

	mbp->bufp = p = (char *) &mbp->ms_report;
	for (i = 0; i < MS_REPORT_SZ; i++)  
		putc(*p++, &tp->t_rawq);
	ttwakeup(tp);
}

#ifdef IBMRTPC
#define RIGHT_BUT	0x80
#define MIDDLE_BUT	0x40
#define LEFT_BUT	0x20
#define NO_BUT		0x00
#define BOTH_BUT	0xA0
#define BUT_MASK 	0xE0
#define BUT_DEFS	"\20\06LEFT\07MIDDLE\08RIGHT"
#endif IBMRTPC

#ifdef ATR
#define RIGHT_BUT	0x02
#define MIDDLE_BUT	0x04
#define LEFT_BUT	0x01
#define NO_BUT		0x00
#define BOTH_BUT	0x03
#define BUT_MASK 	0x07
#define BUT_DEFS	"\20\01LEFT\02RIGHT\03MIDDLE"
#endif ATR

#define SETTIMEOUT(a,b,c) {\
				ptp = tp;\
				timeout(a,b,c);\
			  }

struct tty *ptp;
char msd_debug = 0;

/* 
 * msddecode looks are the state of the buttons and handles the 
 * three button simulation using two buttons.
 * states (single_but):
 * 0	no buttons pressed
 * 1	single button pressed (left or right)
 *	
 * logic:
 * 	if both bottons are pressed then pretend that the middle
 *	button was pressed.
 * states:
 * 1.	not in middle of a single button test, and both buttons were
 *	down and exactly one was let up --> we ignore this transtion
 *	until both are let up.
 * 2.	not in middle of a single button test, and both buttons were
 *	up, and now one is pressed --> arrange to check again in
 *	5 ticks and now in single button test mode.
 * 3.	in the middle of the single button test, and the buttons
 *	are unchanged --> cancel the timeout. if we have been 
 *	gone thru this twice before then assume that the second button
 *	will not be pressed, otherwise arrange another poll.
 * 4.	otherwise, if in single button test (and buttons are different)
 *	then cancel the timeout and the single button mode (e.g.
 *	both buttons are up or both down.
 *
 */

msddecode(flag, mbp, tp)
	register int 	flag;
	register struct mb *mbp;
	struct   tty    *tp;
{
        register mousedata  *pms_report = &mbp->ms_report;
	register char   new_but;
	static		last_but = 0;
	int tty_pollms();

	new_but  = pms_report->m_flags & BUT_MASK; 

	/* Check if this was a poll timeout */
	if (flag == 0) {	/* flag is 0 only when called from tty_pollms */
		if (mbp->single_but) {
			if (++mbp->single_but > 2) 
				mbp->single_but = 0;
			else
				SETTIMEOUT (tty_pollms, mbp, 2);
		}
		return(!mbp->single_but);
	}
	
	/* Check if both buttons are down and simulate a middle button */
	if (new_but == BOTH_BUT)
		new_but = MIDDLE_BUT;	

	if (msd_debug)
		printf ("sb (%x) last (%b) cur (%b)\n", mbp->single_but,
			last_but, BUT_DEFS, new_but, BUT_DEFS);
	if (!mbp->single_but && (last_but != new_but)) {
		if ((last_but == MIDDLE_BUT) && (new_but != NO_BUT)) {
		 	new_but = MIDDLE_BUT;
			mbp->single_but = 0;	/* (1) only one button let up */
		} else if ((last_but == NO_BUT) && (new_but != MIDDLE_BUT)) {
			SETTIMEOUT(tty_pollms, mbp, 4);
			mbp->single_but++;	/* single button pressed down */
		}	/* otherwise single button let up */
	} else if ((mbp->single_but) && (last_but == new_but)) {
		untimeout(tty_pollms, mbp);		/* no change in buttons */
		if (++mbp->single_but > 2)
			mbp->single_but = 0;	/* third time thru */
		else
			SETTIMEOUT (tty_pollms, mbp, 4);
	} else if (mbp->single_but) {		/* change in buttons */
		untimeout(tty_pollms, mbp);
		mbp->single_but = 0;
		if (new_but == NO_BUT) {
			pms_report->m_flags &= ~BUT_MASK; 		/* upate status */
			pms_report->m_flags |= last_but;	/* previous button */
			msreport(mbp,tp);	/* button let up - note both down & up reports */
		}
	}
	pms_report->m_flags &= ~BUT_MASK; 		/* upate status */
	pms_report->m_flags |= new_but;
	last_but = new_but;
	return(!mbp->single_but);
}

tty_pollms (mbp)
register struct mb  *mbp;
{
	if (msd_debug)
		printf ("In tty_pollms\n");
	
	if (msddecode(0, mbp, ptp)) 
		msreport(mbp, ptp);

	/* msioctl (0,MSIC_READXY,0,0); */
}

/*
 * This routine is called whenever a ioctl is about to be performed
 * and gets a chance to reject the ioctl.  We reject all teletype
 * oriented ioctl's except those which set the discipline, and
 * those which get parameters (gtty and get special characters).
 */
/*ARGSUSED*/
msdioctl(tp, cmd, data, flag)
	struct tty *tp;
	caddr_t data;
{

	if ((cmd>>8) != 't')
		return (-1);
	switch (cmd) {

	case TIOCSETD:
	case TIOCGETD:
	case TIOCGETP:
	case TIOCGETC:
		return (-1);
	}
	return (ENOTTY);
}

msdselect(dev, rw)
	dev_t dev;
	int rw;
{
	register struct tty *tp = &cdevsw[major(dev)].d_ttys[minor(dev)];
	int s = MS_SPL;


	switch (rw) {

	case FREAD:
		if (tp->t_rawq.c_cc + tp->t_canq.c_cc > 0)     
			goto win;
		if (tp->t_rsel && tp->t_rsel->p_wchan == (caddr_t)&selwait)
			tp->t_state |= TS_RCOLL;
		else
			tp->t_rsel = u.u_procp;
		break;

	case FWRITE:
		goto win;
	}
	splx(s);
	return (0);
win:
	splx(s);
	return (1);
}
#endif

