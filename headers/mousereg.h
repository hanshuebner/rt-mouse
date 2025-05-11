/*
 * 5799-WZQ (C) COPYRIGHT IBM CORPORATION  1986,1987
 * LICENSED MATERIALS - PROPERTY OF IBM
 * REFER TO COPYRIGHT INSTRUCTIONS FORM NUMBER G120-2083
 */

/* $Header: /usr/src/sys/rtio/RCS/mousereg.h,v 1.3 1991/12/18 20:03:20 md Exp $ */
/* $ACIS:mousereg.h 12.0$ */
/* $Source: /usr/src/sys/rtio/RCS/mousereg.h,v $ */

#if !defined(lint) && !defined(LOCORE)  && defined(RCS_HDRS)
static char *rcsidmousereg = "$Header: /usr/src/sys/rtio/RCS/mousereg.h,v 1.3 1991/12/18 20:03:20 md Exp $";
#endif


/* BAUD RATE TABLE used to specify the baud rate for UART_SET_BAUD
 * calculated ad COUNTER = 256 - (9.216 MHZ / (192 * BAUD))
 */
static struct speedtab msbaudtbl[] = {
	19200,	B19200,
	19200,	EXTB,
	7200,	EXTA,
	9600,	B9600,
	4800,	B4800,
	2400,	B2400,
	1800,	B1800,
	1200,	B1200,
	600,	B600,
	300,	B300,
	200,	B200,
	150,	B150,
	134,	B134,
	110,	B110,
	75,	B75,
	50,	B50,
	0,	B0,
	-1,	-1,
};

/* BAUD_RATE		COUNTER */
#define UART_B24000	254
#define UART_B9600	251
#define UART_B4800	246
#define UART_B2400	236
#define UART_B1200	215
#define UART_B600	176
#define UART_B300	96
#define OSC		9216000		/* Hz */
#define MSBAUD(s)	(256-(OSC/(msbaudrate(s,msbaudtbl)*192)))
#ifdef IBMRTPC
#define	MS_DATA_SYNC	0x0b
#endif IBMRTPC
#ifdef ATR
#define	MS_DATA_SYNC	0x08
#define	MS_SYNC_MASK	0x0c
#endif ATR
#define MS_CONFIGURED	0x20

/* BIT Definitions for UART INITIALIZE FRAMING (UART_INIT_FRM) command
 *     bit 7 high bit
 *
 * Bit	7:	1 = Odd parity (default) ; 0 = Even parity
 *    6-3:	0
 *    2-0:	Blocking Factor 2-6 (4 default) 4 need for data report
 *		from mouse when Blocking is active M5=1
 */
#define UART_FRM_ODDP	0x84
#define UART_FRM_EVENP	0x4
#define	MS_MAX_RETRY	3

#ifdef ATR
/* ATR specific rate defines */
#define MS_RATE_10_ATR	0x00		/* 10 reports/sec	*/
#define MS_RATE_20_ATR	0x01		/* 20 reports/sec	*/
#define MS_RATE_40_ATR	0x02		/* 40 reports/sec	*/
#define MS_RATE_60_ATR	0x03		/* 60 reports/sec	*/
#define MS_RATE_80_ATR	0x04		/* 80 reports/sec	*/
#define MS_RATE_100_ATR	0x05		/* 100 reports/sec	*/
#define MS_RATE_200_ATR	0x06		/* 200 reports/sec	*/

/* ATR specific res defines */
#define MS_RES_200_ATR	0x03	/*	     8		      200	*/
#define MS_RES_100_ATR	0x02	/*	     4		      100	*/
#define MS_RES_50_ATR	0x01	/*	     2		       50	*/
#define MS_RES_25_ATR	0x00	/*	     1		       25	*/
#endif ATR
