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
 *          Author: Weiwu Hu, Weisong Shi, Zhimin Tang                 * 
 * =================================================================== *
 *   This software is ported to SP2 by                                 *
 *                                                                     *
 *         M. Rasit Eskicioglu                                         *
 *         University of Alberta                                       *
 *         Dept. of Computing Science                                  *
 *         Edmonton, Alberta T6G 2H1 CANADA                            *
 * =================================================================== *
 **********************************************************************/

#ifndef JIASYN_H
#define JIASYN_H

#include "comm.h"
#include "global.h"
#define Maxwtnts     511     /*(4096-8)/4/2*/
#define Maxstacksize 8
#define hidelock     Maxlocks
#define top          lockstack[stackptr]
#define bytestoi(x)  (*((int *)(x)))    // get an int from data array in msg
#define stol(x)      (*((unsigned long *) (x))) // reinterpret a pointer into (unsigned long *) and dereference it
#define ltos(x)      ((unsigned char *) (&(x))) // transfer x's address into (unsigned char *)
#define sbit(s,n)    ((s[(n)/8])|=((unsigned char)(0x1<<((n)%8))))      // set the n-th bit to 1
#define cbit(s,n)    ((s[(n)/8])&=(~((unsigned char)(0x1<<((n)%8)))))   // clear the n-th bit (to 0)
#define tbit(s,n)    (((s[(n)/8])&((unsigned char)(0x1<<((n)%8))))!=0)  // test the n-th bit whether equals 1(true) or not(false)
#define WNULL        ((wtnt_t*)NULL)
#define Maxcvs         16       /* maximum number of conditional variables in JIAJIA */


typedef struct wtnttype {
        unsigned char*  wtnts[Maxwtnts];   /*address*/
        int             from[Maxwtnts];   // from pid or from scope
        int             wtntc;            // write notice count
        struct wtnttype *more;
               } wtnt_t;

typedef struct locktype {
        int         acqs[Maxhosts];     /* acquire the lock's hostid*/
        int         acqscope[Maxhosts]; /**/
        int         acqc;               /* acquire counter */
        int         scope;
        int         myscope;
        wtnt_t      *wtntp;             /* write notice list pointer */
               } jialock_t;

typedef struct stacktype {
        int         lockid;     // lock id
        wtnt_t      *wtntp;     // write notice pointer
               } jiastack_t;

typedef struct cvtype {
        int         waits[Maxhosts];  /* hosts waiting on cv */
        int         waitc;            /* number of hosts waiting on cv*/
        int         value;
               } jiacv_t;

/* Function Declaration */
void acqserver(jia_msg_t *req);
void invserver(jia_msg_t *req);
void relserver(jia_msg_t *req);
void wtntserver(jia_msg_t *req);
#endif /*JIASYN_H*/
