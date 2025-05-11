/*
 * 5799-WZQ (C) COPYRIGHT IBM CORPORATION 1986,1987
 * LICENSED MATERIALS - PROPERTY OF IBM
 * REFER TO COPYRIGHT INSTRUCTIONS FORM NUMBER G120-2083
 */
/* $Header: /usr/src/sys/rtcons/RCS/kls.h,v 1.1 1990/10/10 20:41:13 rayan Exp $ */
/* $ACIS:kls.h 12.0$ */
/* $Source: /usr/src/sys/rtcons/RCS/kls.h,v $ */

#if !defined(lint) && !defined(LOCORE)  && defined(RCS_HDRS)
static char *rcsidkls = "$Header: /usr/src/sys/rtcons/RCS/kls.h,v 1.1 1990/10/10 20:41:13 rayan Exp $";
#endif


#define KLS_CNTIW 0x8407		  /* 8255 control port write */
#define KLS_CNTIR 0x8406		  /* 8255 control port read */
#define KLS_READ  0x8404		  /* 8255 data read */
#define KLS_WRITE 0x8400		  /* 8255 data write */
#define KLS_ADAPTOR_RESET 0xFB		  /* adaptor reset */
#define KLS_ADAPTOR_RELEASE 0x04	  /* adaptor release */

#define KBD_BUSY	0x10		  /* Keyboard busy bit           */
#define	KBD_LOCKED	0x20		  /* Keyboard lock bit		*/

#define KLS_INT		0x08		  /* Interrupt Pending Bit */
#define IID_MASK	0x07		  /* mask for Ident code */
#define KLS_INFO	0x00		  /* information */
#define KLS_KBD		0x01		  /* value for data present */
#define KLS_UART	0x02		  /* byte from UART */
#define KLS_REQ		0x03		  /* returned requested byte */
#define KLS_BLOCK	0x04		  /* block request */
#define KLS_RESERVED	0x05		  /* reserved */
#define KLS_SELF_TEST	0x06
#define KLS_ERROR	0x07
#define KLS_TIMEOUT	0x100		  /* if we have timed out */
#define KLSMAXTIME  100000

/* Error return codes (iid= KLS_ERROR) */
#define	KLS_ERROR_LOW	0xe0
#define	KLS_ERROR_HIGH	0xea
#define	KBD_XMIT_TIMEOUT	0xe0
#define	KBD_RCV_TIMEOUT	0xe1
#define	KBD_ACK_TIMEOUT	0xe2
#define	KBD_EXTRA_ACK	0xe3
#define	KBD_RCV_ERROR	0xe4
#define	KBD_XMIT_ERROR	0xe5
#define	MS_NOID		0xe8
#define	MS_XMIT_TIMEOUT	0xe9
#define	MS_ACK_TIMEOUT	0xea

/* Reject (iid=0) codes */
#define	MS_BUSY_REJ	0x4c
#define	KBD_BUSY_REJ	0x42

/*    Adapter commands                                               */
#define		EXTCMD		0x00	  /*  Extended command select  */
#define	          READ_STAT	0x12	  /* Read status byte from shared mem */
#define		  SPKVOLCMD	0x40	  /*  set speaker volume   */
#define		  SETSPKMOD	0x33	  /* Allow speaker info to come in */
#define		  CLRSPKMOD	0x23	  /* Disable spkr complete info */
#define		  SETMSBLK	0x35	  /* Set blocking on           */
#define		  KEYCLICK	0x37	  /* Turn on key clicks     */
#define		  CLRMSBLK	0x25	  /* Set blocking off          */
#define		  SPKREJ	0x44	  /* Reject current speaker cmd */
#define		  KBDLOCK	0x32	  /* Enable interrupt on kbdlockchg */
#define		  NKBDLOCK	0x22	  /* Disable interrupt on kbdlockchg */
#define		UARTCMD		0x04	  /*  Command to uart          */
 /*  These cmds return 2 datablocks                         */
#define           RSMOUSE	0x01	  /*  Reset  mouse             */
#define           QYMOUSE	0x73	  /*  Query  mouse             */
#define		  MSRDCONF	0x06	  /*  Read mouse configuration */
#define		  MSRDDATA	0x0b	  /*  Read mouse datat         */
#define		UARTCNT		0x03	  /*  Control to uart          */
#define           MSTRANS	0x08	  /*  Enable mouse for transm. */
#define		  MSDIS		0x09	  /*  Turn the mouse off       */
#define		  MSWRPON	0x0e	  /*  Set mouse wrap mode on   */
#define		  MSWRPOFF	0x0f	  /*  Set mouse wrap mode off  */
#define		  MSEXPON	0x78	  /*  Set exp reporting on     */
#define		  MSEXPOFF	0x6c	  /*  Set exp reporting off    */
#define		  MSSETSAM	0x8a00	  /*  Set sampling rate (2-byte instr */
#define		  MSSETRES	0x8900	  /*  Set mouse resolution */
#define		  MSSTREAM	0x8d00	  /*  Set streaming mode */
#define		  MSREMOTE	0x8d03	  /*  Set remode mode */
#define		MSBAUDCMD	0x05	  /*  Set mouse baud rate      */
#define		MSFRAME		0x06	  /*  Set mouse framing	       */
#define		KYBDCMD 	0x01	  /*  Command to kybd          */
#define           KRESET	0xFF	  /*  Keyboard reset           */
#define           KDEFDS	0xF5	  /*  Kybd default disable     */
#define           KSCAN		0xF4	  /*  Kybd start scanning      */
#define		    NKSCAN	0xF3	  /*  Turn off kybd            */
#define           READID    0xF2	  /*  Read kybd id             */
#define           SETLED    0xED	  /*  Set kybd LEDs            */
/* These defines will set a list of keys to the associated type */
#define		    KSETMAKE  0xFD		  /*  Set key list to Make  */
#define		    KSETMKBRK 0xFC		  /*  Set make/break */
#define		    KSETRPT   0xFB		  /*  Set repeat [make/break] */
#define		    KSETDBRPT 0xFA		  /*  Double rate repeat/make break */
/* These defines set all keys to the assoctiated type */
#define		    KALLMAKE  0xF9		  /*  Set all keys to Make  */
#define		    KALLMKBRK 0xF8		  /*  All to make/break */
#define		    KALLRPT   0xF7		  /*  All repeat [make/break] */
#define		    KDEAFULT  0xF6		  /*  All keys to default type */
#define		    KTOGMENU  0xF1		  /*  Toggle Keytype Menu      */
#define       WRRAM       0x10		  /*  Write shr RAM            */
#define       RDRAM       0x00		  /*  Read  shr RAM            */
#define       RD1C        0x1C		  /*  Read shr RAM 0x1C.     */
#define       SETFCHI     0x08		  /*  Set freq counter hi byte */
#define         FCHI      0x01		  /*  Hi frequency byte        */
#define       SETFCLO     0x09		  /*  Set freq counter lo byte */
#define         FCLO      0xAC		  /*  Lo frequency byte        */
#define       SPKDURHI    0x07		  /*  Turn on speaker          */
#define       SPKDURLO    0x02		  /*  Turn on speaker          */
#define         SPTIME    0X04		  /*  Speaker duration         */
#define       ENKYBD      0x3B		  /*  Enable kybd mode bit 11  */
#define       ENUART      0x3C		  /*  Enable UART mode bit 12  */
#define       RDVERS      0xE0		  /*  Read version             */
#define       DSLOCK      0x2D		  /*  Disable keylock, bit 13  */
#define       CMDREJ      0x7F		  /*  Command reject received  */
#define       POER        0xFF		  /*  Power-on error report.   */
#define       INFO        0x00		  /*  Int id for information   */
/* Flags to tell set_defualt_key_types what you want */
#define STANDARD_MENU   0
#define ALT_MENU        1

#define HAVE_REPEAT_MKBRK ALT_MENU
#define	KEYCLICK_DEFDUR	0x36
#define	KEYCLICK_AUTO(n)	klscmd(EXTCMD,((n)? 0x2e :0x3e),0)	

#define KLS_CONFIG	0xc3		  /* 8255 configuration value */

/* Keyboard leds */
#define	      NUM_LED     0x04		  /* Num lock led             */
#define	      CAPS_LED    0x02		  /* Caps lock led            */
#define	      SCROLL_LED  0x01		  /* Scroll lock led          */

/*
 * following bits are in KLS_CNTIR
 */
#define KLS_IBF		0x20		  /* IBF (input buffer full?) */
#define KLS_OBF		0x80		  /* OBF (output buffer full?) */
#define CMD(dest,cmd) dest + (cmd <<8)	/* pack data and command */
#define kls_read(queue)	getc(&queue)
#define kls_flush(queue) while (getc(&queue) >= 0)
#define KLSSPL()	_spl2()
#define KLSINVRET	(KLS_RESERVED << 8)
#define KLS_SOFT_ERROR	(KLS_INFO << 8) + 0x7e
#define KLSSRI		0x80		/* unsolicited INFO byte */
#define SPKRDONE	0x40		/* KLSSRI + spkr complete bit */
#define	KEYBDDONE	0x20		/* keyboard command complete */
#define	KEYBDLOCK	0x10		/* keyboard lock changed complete */
#define MSDONE		0x04		/* mouse command complete */
#define KLS_WAIT	1
#define KLS_NOWAIT	0
#define	KLSPOOLSZ	10
#define	CLICKVOL	2
#define	SR_CLICKDUR	0x07
#define SR_SYSATTN_SCAN 0x0f		/* 3rd scan code initiate sys. attn. */

struct klsq {
	struct klsq	*qp_next;	/* pointer to next command on queue */
	char		qp_dest,qp_cmd;	/* destination and command to send */
	int		(* qp_callback)();/* routine to call when completed */
	int		qp_ret;		/* return status of command */
};

#define klsfree(free,qp)	qp->qp_next = free ; free = qp
#define FAIL(n)	TRACEF(("return %d\n",n))
