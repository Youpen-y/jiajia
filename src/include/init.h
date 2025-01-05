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
 *         Author: Weiwu Hu, Weisong Shi, Zhimin Tang                  *
 * =================================================================== *
 *   This software is ported to SP2 by                                 *
 *                                                                     *
 *         M. Rasit Eskicioglu                                         *
 *         University of Alberta                                       *
 *         Dept. of Computing Science                                  *
 *         Edmonton, Alberta T6G 2H1 CANADA                            *
 * =================================================================== *
 **********************************************************************/

#ifndef JIACREAT_H
#define JIACREAT_H

#define Wordsize 80
#define Linesize 200
#define Wordnum 3
#define Maxwords 10
#define Maxfileno                                                              \
    1024 /* maximum number of file descriptors that can be concurrently opened \
            in UNIX, (>= 4*Maxhosts*Maxhosts) */

#define SEGVoverhead 600
#define SIGIOoverhead 200
#define ALPHAsend 151.76
#define BETAsend 0.04
#define ALPHArecv 327.11
#define BETArecv 0.06
#define ALPHA 20
#define BETA 20

/**
 * @brief struct host_t - info of host
 *
 */

typedef struct host {
    int id;            // host id
    char ip[16];       // host ip
    char username[32]; // host username
    char password[32]; // host password
    int homesize;
    int riofd;
    int rerrfd;
} host_t;


#ifdef DOSTAT
typedef struct Stats {
    unsigned int kernelflag;

    unsigned int msgsndbytes;  /*msg send bytes*/
    unsigned int msgrcvbytes;  /*msg recv bytes*/
    unsigned int msgrcvcnt;    /*msg recv count*/
    unsigned int msgsndcnt;    /*msg send count*/
    unsigned int segvLcnt;     /*segv local count*/
    unsigned int segvRcnt;     /*segv remote count*/
    unsigned int usersigiocnt; /*user SIGIO count*/
    unsigned int synsigiocnt;  /*syn SIGIO count*/
    unsigned int segvsigiocnt; /*segv SIGIO count*/
    unsigned int sigiocnt;     /*total SIGIO count*/
    unsigned int barrcnt;      /*barriers count*/
    unsigned int lockcnt;      /*lock count*/
    unsigned int getpcnt;      /*getp msg count*/
    unsigned int diffcnt;      /*diff msg count*/
    unsigned int invcnt;       /*inv msg count*/
    unsigned int mwdiffcnt;    /*MWdiffs count*/
    unsigned int repROcnt;     /*Replaced RO pages*/
    unsigned int repRWcnt;     /*Replaced RW pages*/
    unsigned int migincnt;
    unsigned int migoutcnt;
    unsigned int resentcnt;

    unsigned int segvLtime;
    unsigned int segvRtime;
    unsigned int barrtime;
    unsigned int locktime;
    unsigned int unlocktime;
    unsigned int usersigiotime;
    unsigned int synsigiotime;
    unsigned int segvsigiotime;

    unsigned int endifftime;
    unsigned int dedifftime;

    /*Follow used by Shi*/
    unsigned int asendtime;
    unsigned int difftime;
    unsigned int busytime;
    unsigned int datatime;
    unsigned int syntime;
    unsigned int othertime;
    unsigned int segvtime;
    unsigned int commtime;
    unsigned int commsofttime;
    unsigned int commhardtime;
    unsigned int largecnt; /*large msg count*/
    unsigned int smallcnt; /*small msg count*/
    unsigned int overlapsigiotime;
    unsigned int overlapsigiocnt;
    unsigned int waittime;
} jiastat_t;
#endif

#endif /*JIACREAT_H*/
