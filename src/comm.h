/***********************************************************************
 *                                                                     *
 *   The JIAJIA Software Distributed Shared Memory System              * 
 *                                                                     *
 *   Copyright (C) 1997 the Center of High Performance Computing       * 
 *   of Institute of Computing Technology, Chinese Academy of          * 
 *   Sciences.  All rights reserved.                                   * 
 *                                                                     *
 *   Permission to use, copy, modify and distribute this software      * 
 *   is hereby granted provided that (1) source code retains these     * 
 *   copyright, permission, and disclaimer notices, and (2) redistri-  *
 *   butions including binaries reproduce the notices in supporting    *
 *   documentation, and (3) all advertising materials mentioning       * 
 *   features or use of this software display the following            * 
 *   acknowledgement: ``This product includes software developed by    *
 *   the Center of High Performance Computing, Institute of Computing  *
 *   Technology, Chinese Academy of Sciences."                         * 
 *                                                                     *
 *   This program is distributed in the hope that it will be useful,   *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of    *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.              * 
 *                                                                     *
 *   Center of High Performance Computing requests users of this       * 
 *   software to return to dsm@water.chpc.ict.ac.cn any                * 
 *   improvements that they make and grant CHPC redistribution rights. *
 *                                                                     *
 *           Author: Weiwu Hu, Weisong Shi, Zhimin Tang                * 
 * =================================================================== *
 *   This software is ported to SP2 by                                 *
 *                                                                     *
 *         M. Rasit Eskicioglu                                         *
 *         University of Alberta                                       *
 *         Dept. of Computing Science                                  *
 *         Edmonton, Alberta T6G 2H1 CANADA                            *
 * =================================================================== *
 **********************************************************************/

#ifndef	JIACOMM_H
#define	JIACOMM_H

#include "global.h"
#include "init.h"

#define TIMEOUT      1000
#define MAX_RETRIES  64

#define Maxmsgsize   (40960-Msgheadsize) 
#define Msgheadsize  32
#define Maxmsgs      8 
#define Maxqueue     32			/* size of input and output queue for communication (>= 2*maxhosts)*/

#define  DIFF      0
#define  DIFFGRANT 1     
#define  GETP      2  
#define  GETPGRANT 3     
#define  ACQ       4
#define  ACQGRANT  5    
#define  INVLD     6
#define  BARR      7
#define  BARRGRANT 8
#define  REL       9
#define  WTNT      10 
#define  JIAEXIT   11
#define  WAIT      12
#define  WAITGRANT 13
#define  STAT      14
#define  STATGRANT 15
#define  ERRMSG    16

#define  SETCV     17
#define  RESETCV   18
#define  WAITCV    19
#define  CVGRANT   20
#define  MSGBODY   21
#define  MSGTAIL   22
#define  LOADREQ   23
#define  LOADGRANT 24

#define  BCAST     100

#define  inqh    inqueue[inhead]	// inqueue msg head
#define  inqt    inqueue[intail]	// inqueue msg tail
#define  outqh   outqueue[outhead]	// outqueue msg head
#define  outqt   outqueue[outtail]	// outqueue msg tail


 
typedef struct Jia_Msg {
	unsigned int op;			/* operation type */
	unsigned int frompid;		/* from pid */
	unsigned int topid;			/* to pid */
        unsigned int temp;      /* Useless */
	unsigned int seqno;
        unsigned int index;
        unsigned int scope;     /* Inca. no.  used as tag in msg. passing */
	unsigned int size;			/* data size */
	/* header is 32 bytes */

	unsigned char data[Maxmsgsize];
} jia_msg_t;

typedef  jia_msg_t* msgp_t;

typedef struct CommManager{
    	int                 snd_fds[Maxhosts];		// send file descriptor
   		fd_set              snd_set;				// send fd_set, use with `select`
    	int                 snd_maxfd;				// max_fd, use with `select`
    	unsigned            snd_seq[Maxhosts];		// 

    	int                 rcv_fds[Maxhosts];		// read file descriptor
    	fd_set              rcv_set;				// read fd_set
    	int                 rcv_maxfd;				// max_fd, use with `select`
    	unsigned            rcv_seq[Maxhosts];
} CommManager;


/* function declaration*/
void initcomm();
int req_fdcreate(int, int);
int rep_fdcreate(int, int);

#if defined SOLARIS || defined IRIX62
void    sigio_handler(int sig, siginfo_t *sip, ucontext_t *uap);
#endif /* SOLARIS */
#ifdef LINUX
void    sigio_handler();
#endif
#ifdef AIX41
void    sigio_handler();
#endif /* AIX41 */

void sigint_handler();
void asendmsg(jia_msg_t *msg);
void msgserver();
void outsend();
void bsendmsg(jia_msg_t *msg);
void bcastserver(jia_msg_t *msg);

#endif	/* JIACOMM_H */
