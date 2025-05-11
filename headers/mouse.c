/*
 * 5799-WZQ (C) COPYRIGHT IBM CORPORATION  1986,1987,1988
 * LICENSED MATERIALS - PROPERTY OF IBM
 * REFER TO COPYRIGHT INSTRUCTIONS FORM NUMBER G120-2083
 */
/* $Header: /sys/rt/dev/RCS/mouse.c,v 1.12 1994/05/22 12:26:24 roger Exp $ */
/* $ACIS:mouse.c 12.0$ */
/* $Source: /sys/rt/dev/RCS/mouse.c,v $ */

#if !defined(lint) && !defined(NO_RCS_HDRS)
static char *rcsid = "$Header: /sys/rt/dev/RCS/mouse.c,v 1.12 1994/05/22 12:26:24 roger Exp $";
#endif

/*
 * ibm032 Mouse driver, which works closely with the keyboard
 * code in cons.c
 */
#include "ms.h"
#if NMS > 0

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/user.h>
#include <sys/proc.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/termios.h>
#include <sys/systm.h>
#include <sys/uio.h>
#include "rt/rt/debug.h"
#include <sys/buf.h>
#ifdef IBMRTPC
#include "rt/cons/kls.h"
#endif IBMRTPC
#ifdef ATR
#include <ca_atr/kls.h>
#endif ATR
#include "rt/include/mouseio.h"
#include "rt/dev/mousereg.h"
#include "rt/dev/mousevar.h"
#include "rt/include/consio.h"
#include "rt/include/ioccvar.h"

struct ms_softc ms;
struct mouseq	mscmdq[MSPOOLSZ];
struct mouseq	*msfree;
extern struct clist ms_block,ms_uart;
int msdebug;
int msdebug2;
#ifdef ATR
int msdebug3;
#endif ATR
int msdone(), msparam();

#ifdef IBMRTPC
char ms_reset_ack[] = {0xff, 0x08, 0x00, 0x00};
char ms_reset_ack2[] = {0xff, 0x04, 0x00, 0x00};
#endif IBMRTPC

#ifdef ATR
/*
 * The PS/2 mouse sends 2 bytes on receipt of a reset command.
 *	1st byte : 0xaa = ok,  0xfc = error
 *	2nd byte : always 0x00
 */
char ms_reset_ack[] = {0xaa, 0x00};
#endif ATR

extern int cons_if;
#define MS_NONE		-1	/* generic mouse not open */
static int gen_mouse = MS_NONE;	/* generic mouse unit */
struct tty ms_tty[NUMBER_CONS];

#define MS_UNIT(dev) (minor(dev) & 0x0f)
#define MS_LINE(dev) (minor(dev) >> 4)

short mstype;
#define RT_GENERIC	0x0
#define RT_OPTIC	0x1

#define MS_OPT_CONFIGURED	0x10

int
msbaudrate(code, table)
	unsigned int code;
	struct speedtab table[];
{
	register int i;
	for (i = 0; table[i].sp_code != -1; ++i) {
		if (table[i].sp_code == code)
			return(table[i].sp_speed);
	}
	return(0);
}


/*ARGSUSED*/

msopen(dev, flag)
	dev_t dev;
{
	register struct ms_softc *mssc = &ms;
	register struct tty *tp;
	register int unit = MS_UNIT(dev);
	register int s;
	register int error;
	int generic = 0;
	int ldisc = MS_LINE(dev);

	DEBUGF(msdebug & SHOW_OPEN, printf("In MSOPEN unit=%x\n", unit));

	/* If Generic console minor device then assign this to input focus */
	if (unit == CONS_GEN) {
		if (gen_mouse != MS_NONE)
			return(EBUSY);
		unit = cons_if;
		generic++;
	}

	tp = &ms_tty[unit];
	tp->t_oproc = 0;
	if ((tp->t_state & TS_ISOPEN) == 0) {
		ttychars(tp);
		tp->t_param = msparam;
		tp->t_ospeed = tp->t_ispeed = ISPEED;
		tp->t_state = TS_ISOPEN | TS_CARR_ON;
		tp->t_flags = ODDP;
		if (mssc->ms_cnt <= 0) {
			s = KLSSPL();
			msparam(tp);
			if (reset_mouse(unit)) {
				tp->t_state = 0;
				splx(s);
				return (EIO);
			}
			splx(s);
		}
		mssc->ms_cnt++;
	} else if (generic)
		return (EBUSY);

	DEBUGF(msdebug & SHOW_OPEN, printf("MSOPEN: call ttyopen (%x)\n",
			linesw[tp->t_line].l_open));
	if (error = (*linesw[tp->t_line].l_open)(unit, tp)) {
		msclose(unit);
		return (error);
	}
	if (tp->t_state & TS_XCLUDE && u.u_uid != 0)
		return (EBUSY);
	if (generic)
		gen_mouse = unit;
	if ((ldisc) && (ldisc != tp->t_line))
		msioctl(dev, TIOCSETD, (caddr_t) & ldisc, 0);
#ifdef ATR
	DEBUGF(msdebug & SHOW_OPEN, printf("MSOPEN: returning 0\n"));
#endif ATR
	return (0);
}

/*ARGSUSED*/
msclose(dev)
	dev_t dev;
{
	register int unit = MS_UNIT(dev);
	register struct ms_softc *mssc = &ms;
	register struct tty *tp;
	int generic = 0;
	register int s;

	DEBUGF(msdebug & SHOW_OPEN, printf("In MSCLOSE\n"));

	/* If Generic console minor device then assign this to input focus */
	if (unit == CONS_GEN) {
		unit = gen_mouse;
		generic++;
	}

	tp = &ms_tty[unit];

	if (--mssc->ms_cnt <= 0) {
		s = KLSSPL();
#ifdef IBMRTPC
		ms_cmd(UARTCNT,MSDIS);
#endif IBMRTPC
#ifdef ATR
		ms_cmd(MSCMD_SWITCH, MS_DISABLE);
#endif ATR
		kls_flush(ms_block);
		kls_flush(ms_uart);
		mssc->ms_flags = 0;
#ifdef ATR
		mssc->ms_rate = MS_RATE_100_ATR;
		mssc->ms_resl = MS_RES_100_ATR;
#else
		mssc->ms_rate = MS_RATE_100;
		mssc->ms_resl = MS_RES_100;
#endif
		mssc->msenabled = 0;
		splx(s);
	}
	if ((mssc->ms_sunit == unit) && (mssc->ms_sem != 0)) {
		mssc->ms_flags &= ~MS_BUSY;
		if (mssc->msenabled)
#ifdef IBMRTPC
			ms_cmd(UARTCNT, MSTRANS);
#endif IBMRTPC
#ifdef ATR
			ms_cmd(MSCMD_SWITCH, MS_ENABLE);
#endif ATR
		ms_ssignal(&mssc->ms_sem);
	}
	(*linesw[tp->t_line].l_close)(tp);
	ttyclose(tp);
	tp->t_state = 0;
	if (generic)
		gen_mouse = MS_NONE;
#ifdef IBMRTPC
	DEBUGF(ttydebug & SHOW_OPEN, printf("MSCLOSE end\n"));
#endif IBMRTPC
#ifdef ATR
	DEBUGF(msdebug & SHOW_OPEN, printf("MSCLOSE end\n"));
#endif ATR
}

/*ARGSUSED*/
msread(dev, uio, flag)
	register dev_t dev;
	register struct uio *uio;
	int flag;
{
	register unit = MS_UNIT(dev);
	register struct tty *tp;

	DEBUGF(msdebug & SHOW_RDWR, printf("In MSREAD\n"));

	/* If Generic console minor device then assign this to input focus */
	if (unit == CONS_GEN)
		unit = gen_mouse;
	tp = &ms_tty[unit];

	return ((*linesw[tp->t_line].l_read)(tp, uio, flag));
}


/*
 * The Mouse wants to give us a character.
 * Catch the character, and see who it goes to.
 */
/*ARGSUSED*/
msrint()
{
	register int c;
	register struct ms_softc *mssc = &ms;
	register struct tty *tp = &ms_tty[cons_if];

	DEBUGF(msdebug2,printf("In msrint, flags = %x\n",ms.ms_flags););
	/* if MS_QUERY is set someone wants to get at the mouse directly */
	if(mssc->ms_flags & MS_QUERY) {
		mssc->ms_flags &= ~MS_QUERY;
		wakeup( (caddr_t) &mssc->ms_flags);
		DEBUGF(msdebug2,printf("wakeup called\n"););
		return;
	}
	/* if MS_BUSY then someone is still using the input */
	if (mssc->ms_flags & MS_BUSY)
		return;
	/*
	 * Otherwise we aren't interested in ms_block, just get the data and 
	 * send it to the line disipline.
  	 */
	kls_flush(ms_block);
	/* If the mouse is closed, no one is interested */
	if((tp->t_state & TS_ISOPEN) == 0) {
#ifdef ATR
		DEBUGF(msdebug3,printf("MSRINT: mouse tty closed, flushing ms_uart\n"););
#endif ATR
		kls_flush(ms_uart);
		return;
	}
	while((c=kls_read(ms_uart)) != -1) {
#ifdef ATR
		DEBUGF(msdebug3,printf("MSRINT: sending ch=0x%x to linesw\n",c));
#endif ATR
		(*linesw[tp->t_line].l_rint)(c,tp);
	}
}

/*ARGSUSED*/
msioctl(dev, cmd, addr, flag)
	dev_t dev;
	caddr_t addr;
{
	register struct ms_softc *mssc = &ms;
	register struct tty *tp; 
	register int unit = MS_UNIT(dev);
	register int error = 0;
	register int *mssemaphore = &ms.ms_sem;
	int	s;

	s = KLSSPL();

	if (unit == CONS_GEN)
		unit = gen_mouse;

	tp = &ms_tty[unit];

	DEBUGF(msdebug,printf("in MSIOCTL(dev=%x, cmd=%x, addr=%x, flag=%x)\n",
		dev, cmd, addr, flag));
	switch (cmd) {
#ifdef IBMRTPC
	case MSIC_STREAM:
		/* Set To STREAM mode (Two byte command) */
		ms_cmd(UARTCNT, MSSTREAM);
		mssc->ms_flags &= ~MS_NOSTREAM;
		break;
	case MSIC_REMOTE:
		/* Set To REMOTE mode (Two byte command) */
		ms_cmd(UARTCNT, MSREMOTE);
		mssc->ms_flags |= MS_NOSTREAM;
		break;
#endif IBMRTPC
	case MSIC_STATUS:
		/* Give the user the current raw status of the mouse */
		msstatus(unit,addr);
		break;
#ifdef IBMRTPC
	case MSIC_READXY:
		ms_swait(unit,mssemaphore);

		/* Force a read request to the mouse and go get the data */
		if (mssc->msenabled)
			ms_cmd(UARTCNT, MSDIS);
		kls_flush(ms_block);
		kls_flush(ms_uart);
		ms_cmd(UARTCMD, MSRDDATA);
		msrint();
		if (mssc->msenabled)
			ms_cmd(UARTCNT, MSTRANS);
		ms_ssignal(mssemaphore);
		break;
#endif IBMRTPC
	case MSIC_ENABLE:
		/* Enable the mouse to send data in stream mode */
#ifdef IBMRTPC
		ms_cmd(UARTCNT, MSTRANS);
#endif IBMRTPC
#ifdef ATR
		ms_cmd(MSCMD_SWITCH, MS_ENABLE);
#endif ATR
		mssc->msenabled = 1;
		break;
	case MSIC_DISABLE:
		/* Disable the mouse from sending data in stream mode */
		ms_swait(unit,mssemaphore);
#ifdef IBMRTPC
		ms_cmd(UARTCNT, MSDIS);
#endif IBMRTPC
#ifdef ATR
		ms_cmd(MSCMD_SWITCH, MS_DISABLE);
#endif ATR
		mssc->msenabled = 0;
		kls_flush(ms_block);
		kls_flush(ms_uart);
		ms_ssignal(mssemaphore);
		break;
	case MSIC_EXP:
#ifdef IBMRTPC
		/* Turn on Exponential scaling on the mouse */
		ms_cmd(UARTCNT, MSEXPON);
#endif IBMRTPC
#ifdef ATR
		/* Turn on 2:1 scaling on the mouse */
		ms_cmd(MSCMD_EXTEND, MS_2TO1);
#endif ATR
		mssc->ms_flags |= MS_EXP;
		break;
	case MSIC_LINEAR:
		/* Turn on Linear scaling on the mouse */
#ifdef IBMRTPC
		ms_cmd(UARTCNT, MSEXPOFF);
#endif IBMRTPC
#ifdef ATR
		ms_cmd(MSCMD_EXTEND, MS_1TO1);
#endif ATR
		mssc->ms_flags &= ~MS_EXP;
		break;
	case MSIC_SAMP:
		{
			/* Set the sampling rate used for the mouse */
			long	tmp = *(long *)addr;
#ifdef IBMRTPC
			DEBUGF(msdebug,
				printf("Set samp (%x)\n", MSSETSAM | tmp);
			);
			ms_cmd(UARTCNT, MSSETSAM | tmp);
#endif IBMRTPC
#ifdef ATR
			switch (tmp) {
			case MS_RATE_10:
				tmp = MS_RATE_10_ATR;
				break;
			case MS_RATE_20:
				tmp = MS_RATE_20_ATR;
				break;
			case MS_RATE_40:
				tmp = MS_RATE_40_ATR;
				break;
			case MS_RATE_60:
				tmp = MS_RATE_60_ATR;
				break;
			case MS_RATE_80:
				tmp = MS_RATE_80_ATR;
				break;
			case MS_RATE_100:
				tmp = MS_RATE_100_ATR;
				break;
			default:
				tmp = MS_RATE_100_ATR;
				break;
			}
			DEBUGF(msdebug, printf("Set samp (mscmd %x)  (rate %x)\n",
				MSCMD_SETRATE, tmp);
			);
			ms_cmd(MSCMD_SETRATE, tmp);
#endif ATR
			mssc->ms_rate = tmp;
		}
		break;
	case MSIC_RESL:
		{
			/* Set the resolution the mouse uses */
			long	tmp = *(long *) addr;
#ifdef IBMRTPC
			DEBUGF(msdebug,
				printf("Set resl (%x)\n", MSSETRES |tmp);
			);
			ms_cmd(UARTCNT, MSSETRES | tmp);
#endif IBMRTPC
#ifdef ATR
			switch (tmp) {
			case MS_RES_25:
				tmp = MS_RES_25_ATR;
				break;
			case MS_RES_50:
				tmp = MS_RES_50_ATR;
				break;
			case MS_RES_100:
				tmp = MS_RES_100_ATR;
				break;
			case MS_RES_200:
				tmp = MS_RES_200_ATR;
				break;
			default:
				tmp = MS_RES_100_ATR;
				break;
			}
			DEBUGF(msdebug, printf("Set resl (mscmd %x)  (resl %x)\n",
				MSCMD_SETRES, tmp);
		);
			ms_cmd(MSCMD_SETRES, tmp);
#endif ATR
			mssc->ms_resl = tmp;
		}
		break;
	default:
		/* TTY ioctls */
		splx(s);
		error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, addr, flag);
		if (error >= 0)
			return (error);
		error = ttioctl(tp, cmd, addr, flag);
		if (error < 0)
			error = ENOTTY;
	}
	splx(s);
	return (error);
}

msselect (dev, rw)
	dev_t dev;
	int rw;
{
	register int unit = MS_UNIT(dev);
	register struct tty *tp;

	/* If Generic console minor device then assign this to input focus */
	if (unit == CONS_GEN)
		unit = gen_mouse;

	/* Rebuild dev with new minor device number */
	dev = (dev & 0xff00) | unit;

	/*
	 * Tablets don't queue things in the same way as normal tty stuff
	 * so need to call its specific routine if it is a tablet. Must call
	 * explicitly since there isn't a select entry in linesw.
	 */
	tp = &ms_tty[unit];
	if(tp->t_line == TABLDISC)
		return(tbselect(dev, rw));
	else
		return ttselect(dev, rw);
}

msparam(tp)
	register struct tty *tp;
{
#ifdef IBMRTPC
	register int x;
	register int s;

	s = KLSSPL();

	DEBUGF(msdebug & SHOW_IO, {
		printf("In MSPARAM\n");
		printf("tp->t_flags = (%x)\n", tp->t_flags);
	}
	);

	/* Enable UART interface with a clear */
	x = ms_cmd(EXTCMD, ENUART);
	if (x != 0x0000) {
		printf("msparam: uart did not enable (%x)\n", x);
	}
	/* Set the baud rate and initial mouse settings */
	DEBUGF(msdebug & SHOW_IO, printf("MSPARAM: Setting baud (%d)\n",
		MSBAUD(tp->t_ispeed));
	);
	x = ms_cmd(MSBAUDCMD, MSBAUD(tp->t_ispeed));
	if (x != 0x0000) {
		printf("msparam: uart didn't Set baud rate (%x)\n", x);
	}
	/* Set the baud rate and initial mouse settings */
	x = ms_cmd(MSFRAME,(tp->t_flags & ODDP) ? UART_FRM_ODDP : UART_FRM_EVENP);
	if (x != 0x0000) {
		printf("msparam: uart didn't init framing (%x)\n", x);
	}
	DEBUGF(msdebug & SHOW_IO, printf(" msparam end\n"));

	splx(s);
#endif IBMRTPC
}

msstatus(unit,arg)
	register char *arg;
{
	register int x,i;
	register int *mssemaphore = &ms.ms_sem;
	int	s;

	ms_swait(unit,mssemaphore);
#ifdef IBMRTPC
	if (ms.msenabled)
		ms_cmd(UARTCNT,MSDIS);

	kls_flush(ms_uart);
	kls_flush(ms_block);
	s=KLSSPL();
	ms.ms_flags |= (MS_QUERY | MS_BUSY);
	ms_cmd(UARTCMD, QYMOUSE);
	while (ms.ms_flags & MS_QUERY)
		tsleep((caddr_t) &ms.ms_flags,MSPRI);
	if ((x=kls_read(ms_block)) != 4)
		printf("msstatus: Bad block count %d\n",x);
	for (i=0; (i < 4) && ((x = kls_read(ms_uart)) >= 0); i++)
		*arg++ = (char) x;
	if (i != 4)
		printf("msstatus: Not all status read\n");
	ms.ms_flags &= ~MS_BUSY;
	if (ms.msenabled)
		ms_cmd(UARTCNT, MSTRANS);
#endif IBMRTPC

#ifdef ATR
	if (ms.msenabled)
		ms_cmd(MSCMD_SWITCH, MS_DISABLE);

	kls_flush(ms_uart);
	s=KLSSPL();
	ms.ms_flags |= (MS_QUERY | MS_BUSY);
	ms_cmd(MSCMD_EXTEND, MS_QSTATUS);
	while (ms.ms_flags & MS_QUERY)
		tsleep((caddr_t) &ms.ms_flags,MSPRI);
	for (i=0; (i < 3) && ((x = kls_read(ms_uart)) >= 0); i++)
		*arg++ = (char) x;
	if (i != 3)
		printf("msstatus: Not all status read\n");
	ms.ms_flags &= ~MS_BUSY;
	if (ms.msenabled)
		ms_cmd(MSCMD_SWITCH, MS_ENABLE);
#endif ATR
	ms_ssignal(mssemaphore);
	splx(s);
}

reset_mouse(unit)
{
#ifdef IBMRTPC
	register int i;
	register int retry = 0;
	register char x;
	register int *mssemaphore = &ms.ms_sem;
	int s;
	int bogus;

	mstype = RT_GENERIC;	/* assumption */

	/* Enable Blocking mode on UART */
	if (ms_cmd(EXTCMD, SETMSBLK)) {
		printf("reset_mouse: uart didn't Set block mode\n");
		return (1);
	}
	ms_swait(unit,mssemaphore);
	/* Reset the MOUSE */
	do {
		kls_flush(ms_uart);
		kls_flush(ms_block);
		s=KLSSPL();
		ms.ms_flags |= (MS_QUERY | MS_BUSY);
		if (ms_cmd(UARTCMD, RSMOUSE)) {
			printf("reset_mouse: mouse didn't accept reset\n");
			splx(s);
			continue;
		}
		while (ms.ms_flags & MS_QUERY)
			tsleep( (caddr_t) &ms.ms_flags,MSPRI);
		splx(s);
		if ((x=kls_read(ms_block)) != 4) {
			printf("reset_mouse: Bad block count = %d\n",x);
			ms.ms_flags &= ~MS_BUSY;
			continue;
		}
		for (i = 0, bogus = 0; i < 4; i++) {
			/* XXX: change this to a switch stmt */
			if ((x = kls_read(ms_uart)) != ms_reset_ack[i]) {
			    if (x != ms_reset_ack2[i]) {
				bogus ++;
/* 				break; */
			    }
			    else {
				mstype = RT_OPTIC;
			    }
			}
#if 0
			printf ("mouse = %x -- %x = reset_ack[%d]\n",
				x, ms_reset_ack[i], i);
#endif
		}
		ms.ms_flags &= ~MS_BUSY;
		if (i == 4 && !bogus)
			break;
	} while (retry++ < MS_MAX_RETRY);

	if (retry >= MS_MAX_RETRY) {
		printf("reset_mouse: Mouse didn't reset (%x)\n", x);
		ms_ssignal(mssemaphore);
		return (1);
	}
	/* Disable Blocking mode on UART */
	if (ms_cmd(EXTCMD, CLRMSBLK)) {
		printf("reset_mouse: uart didn't Reset block mode\n");
		ms_ssignal(mssemaphore);
		return (1);
	}
	/* Read Configuration */
	s=KLSSPL();
	ms.ms_flags |= (MS_QUERY | MS_BUSY);
	if (ms_cmd(UARTCMD, MSRDCONF)) {
		printf("reset_mouse: mouse didn't accept read config\n");
		ms_ssignal(mssemaphore);
		return (1);
	}
	while (ms.ms_flags & MS_QUERY)
		tsleep( (caddr_t) &ms.ms_flags,MSPRI);
	splx(s);

	if ((x = kls_read(ms_uart)) != 
	    (mstype == RT_GENERIC ? MS_CONFIGURED : MS_OPT_CONFIGURED)) {
		printf("reset_mouse: Wrong configuration resp (%x)\n", x);
		ms.ms_flags &= ~MS_BUSY;
		ms_ssignal(mssemaphore);
		return (1);
	}
	ms.ms_flags &= ~MS_BUSY;

	/* Enable Blocking mode on UART */
	if (ms_cmd(EXTCMD, SETMSBLK)) {
		printf("reset_mouse: uart didn't Set block mode\n");
		ms_ssignal(mssemaphore);
		return (1);
	}
	/* Enable data transmissions */
	if (ms_cmd(UARTCNT, MSTRANS)) {
		printf("reset_mouse: mouse didn't accept enable cmd\n");
		ms_ssignal(mssemaphore);
		return (1);
	}
	ms.msenabled = 1;
	ms_ssignal(mssemaphore);

#endif IBMRTPC

#ifdef ATR
	kls_flush(ms_uart);

	/* Enable data transmissions */
	if (ms_cmd(MSCMD_SWITCH, MS_ENABLE)) {
		printf("reset_mouse: mouse didn't accept enable cmd\n");
	/*	ms_ssignal(mssemaphore);	*/
		return (1);
	}
	if (ms.ms_rate != MS_RATE_100_ATR)
		ms_cmd(MSCMD_SETRATE, ms.ms_rate);
	if (ms.ms_resl != MS_RES_100_ATR)
		ms_cmd(MSCMD_SETRES, ms.ms_resl);
	DEBUGF(msdebug2,printf("reset_mouse: mouse enabled, returning(0)\n"););
#endif ATR

	return (0);
}

#ifdef IBMRTPC
ms_cmd(dest,cmd)
{
	register struct	mouseq	 *qp;
	register int	s;
	int	 ret;

	s=KLSSPL();
	DEBUGF(msdebug2,printf("In mscmd, dest=0x%x, cmd=0x%x\n",dest,cmd););
	while( !(qp = (struct mouseq *) klsalloc(&msfree)) )
		tsleep( (caddr_t) &msfree, MSPRI);
	qp->ms_count = 0;
	qp->cmd1.qp_ret = KLSINVRET;
	DEBUGF(msdebug,printf("Got queue\n"););
	if (cmd & 0xff00) {
		qp->cmd1.qp_dest = dest;
		qp->cmd1.qp_cmd = cmd >> 8;
		qp->cmd1.qp_callback = msdone;
		qp->ms_count = 1;
	}
	qp->cmd2.qp_dest = dest;
	qp->cmd2.qp_cmd = cmd & 0xff;
	qp->cmd2.qp_callback = msdone;
	qp->cmd2.qp_ret = KLSINVRET;
	msstrategy(qp);
	while ((qp->cmd2.qp_ret == KLSINVRET) || (qp->cmd2.qp_ret == MS_BUSY_REJ))
		tsleep( (caddr_t) qp,MSPRI);
	ret=qp->cmd2.qp_ret;
	klsfree(msfree,qp);
	wakeup( (caddr_t) &msfree);
	splx(s);
	return(ret);
}
#endif IBMRTPC

#ifdef ATR
/*
 *  Revised considerably for ATR version.
 *
 *  We use the existing kls command queueing mechanism with some
 *  revisions.  There are no two byte commands so ms_count = 0 always
 *  and we only fill out one klsq structure (cmd2) for each command.
 *  All mouse commands are passed to BIOS which use a single command
 *  code and no more than one parameter.  We eliminate the 'dest'
 *  parameter and pass the command code in 'cmd' and the parameter
 *  in 'param'.  The parameter is queued in the qp_param field added
 *  to the klsq structure (see ../ca_atr/kls.h).
 */
ms_cmd(cmd,param)
{
	register struct	mouseq	 *qp;
	register int	s;
	int	 ret;

	s=KLSSPL();
	DEBUGF(msdebug2,printf("In MS_CMD, cmd=0x%x, param=0x%x\n",cmd,param););
	while( !(qp = (struct mouseq *) klsalloc(&msfree)) )
		tsleep( (caddr_t) &msfree, MSPRI);
	qp->ms_count = 0;
	DEBUGF(msdebug,printf("MS_CMD: Got queue\n"););

	qp->cmd2.qp_dest = MSCMD;
	qp->cmd2.qp_cmd = cmd & 0xff;
	qp->cmd2.qp_param = param & 0xff;
	qp->cmd2.qp_callback = msdone;
	qp->cmd2.qp_ret = KLSINVRET;
	msstrategy(qp);
	while ((qp->cmd2.qp_ret == KLSINVRET) || (qp->cmd2.qp_ret == MS_BUSY_REJ)) {
		DEBUGF(msdebug,printf("MS_CMD: bad return code, sleeping,qp_ret=0x%x\n", qp->cmd2.qp_ret););
		tsleep( (caddr_t) qp,MSPRI);
	}
	ret=qp->cmd2.qp_ret;
	DEBUGF(msdebug,printf("MS_CMD: return code, qp_ret=0x%x\n", ret););

	klsfree(msfree,qp);
	wakeup( (caddr_t) &msfree);
	splx(s);
	return(ret);
}
#endif ATR

msstrategy(qp)
	register struct	mouseq	*qp;
{
	register struct ms_softc *mssc = &ms;

	qp->qp_next = NULL;
	if (mssc->ms_head == NULL) {
		mssc->ms_head = qp;
		mssc->ms_tail = qp;
#ifdef ATR
		DEBUGF(msdebug,printf("MSSTRATEGY: calling msstart, qp=0x%x\n", qp););
#endif ATR
		msstart();
	} else {
		mssc->ms_tail->qp_next=qp;
		mssc->ms_tail=qp;
	}
}

msdone(qp)
	register struct klsq *qp;
{
	register struct ms_softc *mssc= &ms;
	register struct mouseq *current;

	DEBUGF(msdebug,printf("In mssdone, cmd=0x%x, data=0x%x, ret=0x%x\n",
		qp->qp_dest,qp->qp_cmd,qp->qp_ret););

	if ((current=mssc->ms_head) == NULL) {
		panic("msdone.");
	}
	/*
	 * User mode machine check encountered. Reset the current command
	 * to the nearest full command and let kbdreset restart
	 */
	if ((qp->qp_ret) == KLS_SOFT_ERROR) {
		if ((current->ms_count == 0) && 
			(current->cmd1.qp_ret != KLSINVRET))
			current->ms_count = 1;
		return;
	}

	if (qp->qp_ret == MS_BUSY_REJ) {
		DEBUGF(msdebug2,printf("msbusy\n"););
		msstart();
		return;
	} 

#ifdef DEBUG
	if (qp->qp_ret)
		printf("mssdone: 0x%x from dest 0x%x, cmd 0x%x.\n",
			qp->qp_ret,qp->qp_dest,qp->qp_cmd);
#endif DEBUG

	/* find next command */
	if ((--current->ms_count) < 0) {
		mssc->ms_head = current->qp_next;
		wakeup( (caddr_t) current);
		if (mssc->ms_head != NULL)
			msstart();
	} else 
		msstart();
}

msstart()
{
	register struct mouseq *mshead;

	if ((mshead = ms.ms_head) == NULL)
		return;
	if (mshead->ms_count == 1)
		klsstrategy(&mshead->cmd1);
	else if (mshead->ms_count == 0) {
#ifdef ATR
		DEBUGF(msdebug,printf("MSSTART: calling klsstrategy\n"));
#endif ATR
		klsstrategy(&mshead->cmd2);
	}
	else
		panic("msstart");
}
	
		
msreset()
{
	register struct ms_softc	*mssc = &ms;
#ifdef IBMRTPC
	register int x;
	register struct tty *tp = &ms_tty[cons_if];
	register int i;
	register int retry = 0;
	int iid;

	if (mssc->ms_cnt > 0) {
		/* Enable UART interface with a clear */
		if (x=kls_raw_cmd(EXTCMD, ENUART))
			printf("msreset: uart did not enable (%x)\n", x);
		/* Set baud rate and parity */
		if (x=kls_raw_cmd(MSBAUDCMD, MSBAUD(tp->t_ispeed)))
			printf("msreset: uart didn't Set baud rate (%x)\n", x);
		if (x=kls_raw_cmd(MSFRAME,(tp->t_flags & ODDP) ? UART_FRM_ODDP : UART_FRM_EVENP))
			printf("msreset: uart didn't init framing (%x)\n", x);

		/* Enable Blocking mode on UART */
		if (kls_raw_cmd(EXTCMD, SETMSBLK))
			printf("msreset: uart didn't Set block mode\n");
		/* Reset the MOUSE */
		do {
			kls_pflush(10);
			if (kls_raw_cmd(UARTCMD, RSMOUSE)) {
				printf("msreset: mouse didn't accept reset\n");
				continue;
			}
			if ((x=kls_raw_read(&iid)) != 4) {
				printf("msreset: Bad block count = %d\n",x);
				continue;
			}
			for (i = 0; i < 4; i++)
			    if ((x = kls_raw_read(&iid)) != ms_reset_ack[i]) {
				if (x != ms_reset_ack2[i]) {
				    break;
				}
			    }
			if (i == 4)
				break;
		} while (retry++ < MS_MAX_RETRY);

		if (retry >= MS_MAX_RETRY)
			printf("msreset: Mouse didn't reset (%x)\n", x);
		/* Disable Blocking mode on UART */
		if (kls_raw_cmd(EXTCMD, CLRMSBLK))
			printf("msreset: uart didn't Reset block mode\n");
		/* Read Configuration */
		if (kls_raw_cmd(UARTCMD, MSRDCONF))
			printf("msreset: mouse didn't accept read config\n");
		if ((x = kls_raw_read(&iid)) != MS_CONFIGURED)
			printf("msreset: Wrong configuration resp (%x)\n", x);
		/* Enable Blocking mode on UART */
		if (kls_raw_cmd(EXTCMD, SETMSBLK))
			printf("msreset: uart didn't Set block mode\n");

		if (mssc->ms_flags & MS_NOSTREAM)
			if (kls_raw_cmd(UARTCNT,MSREMOTE))
				printf("msreset: couldn't set remote mode\n");
		if (mssc->ms_flags & MS_EXP)
			if (kls_raw_cmd(UARTCNT,MSEXPON))
				printf("msreset: couldn't set exponential mode\n");
		if (mssc->ms_rate != MS_RATE_100)
		{
			if ((x=kls_raw_cmd(UARTCNT,MSSETSAM >> 8)) == 0) 
				while((x=kls_raw_cmd(UARTCNT,mssc->ms_rate)) == MS_BUSY_REJ)
					/* void */;
			if (x)
				printf("msreset: couldn't set sampling rate\n");
		}
		if (mssc->ms_resl != MS_RES_100)
		{
			if ((x=kls_raw_cmd(UARTCNT,MSSETRES >> 8)) == 0) 
				while((x=kls_raw_cmd(UARTCNT,mssc->ms_resl)) == MS_BUSY_REJ)
					/* void */;
			if (x)
				printf("msreset: couldn't set resolution\n");
		}
		
	}
	if (mssc->msenabled)
		klscmd(UARTCNT,MSTRANS,0);
#endif IBMRTPC

#ifdef ATR
	DEBUGF(msdebug, printf("In MSRESET \n"));
	if (mssc->ms_head != NULL) {
		DEBUGF(msdebug, printf("In MSRESET: calling msstart() \n"));
		msstart();
	}
#endif ATR
}

msinit()
{
	register struct ms_softc	*mssc = &ms;

	msfree = (struct mouseq *) klsminit(mscmdq,sizeof(struct mouseq),MSPOOLSZ);
	mssc->ms_sem = 0;
	mssc->ms_flags = 0;
#ifdef ATR
	mssc->ms_rate = MS_RATE_100_ATR;
	mssc->ms_resl = MS_RES_100_ATR;
#else
	mssc->ms_rate = MS_RATE_100;
	mssc->ms_resl = MS_RES_100;
#endif
	
}

ms_swait(unit,semaphore)
	int	unit;
	int	*semaphore;
{
	int	s=KLSSPL();

#ifdef ATR
	DEBUGF(msdebug,printf("In MS_SWAIT\n"));
#endif ATR
	while ((*semaphore) < 0)
		tsleep( (caddr_t) semaphore,MSPRI);
	ms.ms_sunit = unit;
	(*semaphore)--;
	splx(s);
}

ms_ssignal(semaphore)
	int	*semaphore;
{
	int	s=KLSSPL();

#ifdef ATR
	DEBUGF(msdebug,printf("In MS_SSIGNAL\n"));
#endif ATR
	(*semaphore)++;
	wakeup( (caddr_t) semaphore);
	splx(s);
}

mswait()
{
	while (ms.ms_head != NULL)
		delay(1);		/* for hc2 */
}

#ifdef ATR
mssuspend(how)
{
	register struct ms_softc	*mssc = &ms;
	register int unit = cons_if;
	register struct tty *tp = &ms_tty[unit];


	if (how == SUSPEND_DONE && mssc->ms_cnt > 0) {
		msparam(tp);
		if (reset_mouse(unit))
			printf("ms: suspend couldn't reset mouse\n");
	}
}
#endif ATR
#endif NMS

