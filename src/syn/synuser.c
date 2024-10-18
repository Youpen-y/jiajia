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

#ifndef NULL_LIB
#include "comm.h"
#include "global.h"
#include "init.h"
#include "jia.h"
#include "mem.h"
#include "syn.h"
#include "tools.h"
#include "utils.h"
#include "setting.h"
#include "stat.h"

#ifdef DOSTAT
extern int statflag;
extern jiastat_t jiastat;
#endif

/* syn */
extern float caltime, starttime, endtime;
extern int H_MIG, LOAD_BAL, W_VEC;
extern volatile int waitwait, cvwait, acqwait, barrwait;
extern int stackptr;
extern jiastack_t lockstack[Maxstacksize]; // lock stack

static void pushstack(int lock);
static void popstack();

/************Lock Part****************/

/**
 * @brief jia_lock -- get the lock
 *
 * @param lock
 */
void jia_lock(int lock) {
    int i;
#ifdef DOSTAT
    register unsigned int begin = get_usecs();
    if (statflag == 1) {
        jiastat.lockcnt++;
        jiastat.kernelflag = 1;
    }
#endif

    if (system_setting.hostc == 1)
        return; // single host no need to use lock

    if (LOAD_BAL == ON) {
        endtime = jia_clock();
        caltime += (endtime - starttime);
    }

    jia_assert(((lock >= 0) && (lock < Maxlocks)),
           "lock %d should range from 0 to %d", lock, Maxlocks - 1);

    for (i = 0; i <= stackptr; i++)
        jia_assert((lockstack[i].lockid != lock),
               "Embeding of the same lock is not allowed!");

    endinterval(ACQ);

    acqwait = 1;
    acquire(lock);

    startinterval(ACQ);

    pushstack(lock);

    if (LOAD_BAL == ON)
        starttime = jia_clock();

#ifdef DOSTAT
    if (statflag == 1) {
        jiastat.locktime += get_usecs() - begin;
        jiastat.kernelflag = 0;
    }
#endif
}

/**
 * @brief jia_unlock --
 *
 * @param lock
 */
void jia_unlock(int lock) {
#ifdef DOSTAT
    register unsigned int begin = get_usecs();
    if (statflag == 1) {
        jiastat.kernelflag = 1;
    }
#endif
    if (system_setting.hostc == 1)
        return;

    if (LOAD_BAL == ON) {
        endtime = jia_clock();
        caltime += (endtime - starttime);
    }

    jia_assert(((lock >= 0) && (lock < Maxlocks)),
           "lock %d should range from 0 to %d", lock, Maxlocks - 1);

    jia_assert((lock == top.lockid), "lock and unlock should be used in pair!");

    endinterval(REL);

    sendwtnts(REL);

    startinterval(REL);

    popstack();

    if (LOAD_BAL == ON)
        starttime = jia_clock();

#ifdef DOSTAT
    if (statflag == 1) {
        jiastat.unlocktime += get_usecs() - begin;
        jiastat.kernelflag = 0;
    }
#endif
}


/************Barrier Part****************/

/**
 * @brief jia_barrier -- end an interval and start next
 *
 */
void jia_barrier() {
#ifdef DOSTAT
    register unsigned int begin = get_usecs();
    if (statflag == 1) {
        jiastat.barrcnt++;
        jiastat.kernelflag = 1;
    }
#endif
    if (system_setting.hostc == 1)
        return; // single machine

    if (LOAD_BAL == ON) {
        endtime = jia_clock();
        caltime += (endtime - starttime);
    }

   jia_assert((stackptr == 0), "barrier can not be used in CS!");

    VERBOSE_LOG(3, "\nEnter jia barrier\n");
    endinterval(BARR);

    if (H_MIG == ON) {
        migcheckcache();
    }

    barrwait = 1;
    sendwtnts(BARR);
    freewtntspace(top.wtntp);
    while (barrwait)
        ;
    VERBOSE_LOG(3, "555555555555555\n");
    if ((H_MIG == ON) && (W_VEC == ON)) {
        jia_wait();
    }
    startinterval(BARR);
    VERBOSE_LOG(3, "66666666666666\n");
    if (LOAD_BAL == ON)
        starttime = jia_clock();

#ifdef DOSTAT
    if (statflag == 1) {
        jiastat.barrtime += get_usecs() - begin;
        jiastat.kernelflag = 0;
    }
#endif
    VERBOSE_LOG(3, "jia_barrier completed\n");
}


/************Wait Part****************/

/**
 * @brief jia_wait -- send WAIT msg to master and wait for WAITGRANT reply to be
 * served
 *
 */
void jia_wait() {
    jia_msg_t *req;

    if (system_setting.hostc == 1)
        return;

    if (LOAD_BAL == ON) {
        endtime = jia_clock();
        caltime += (endtime - starttime);
    }

    req = newmsg();
    req->frompid = system_setting.jia_pid;
    req->topid = 0;
    req->op = WAIT;
    req->size = 0;

    waitwait = 1;
    asendmsg(req);
    freemsg(req);
    while (waitwait)
        ;

    if (LOAD_BAL == ON)
        starttime = jia_clock();
}


/************Conditional Variable Part****************/

void jia_setcv(int cvnum) {
    jia_msg_t *req;

    if (system_setting.hostc == 1)
        return;

    jia_assert(((cvnum >= 0) && (cvnum < Maxcvs)),
           "condv %d should range from 0 to %d", cvnum, Maxcvs - 1);

    req = newmsg();
    req->op = SETCV;
    req->frompid = system_setting.jia_pid;
    req->topid = cvnum % system_setting.hostc;
    req->size = 0;
    appendmsg(req, ltos(cvnum), Intbytes);

    asendmsg(req);

    freemsg(req);
}

void jia_resetcv(int cvnum) {
    jia_msg_t *req;

    if (system_setting.hostc == 1)
        return;

    jia_assert(((cvnum >= 0) && (cvnum < Maxcvs)),
           "condv %d should range from 0 to %d", cvnum, Maxcvs - 1);

    req = newmsg();
    req->op = RESETCV;
    req->frompid = system_setting.jia_pid;
    req->topid = cvnum % system_setting.hostc;
    req->size = 0;
    appendmsg(req, ltos(cvnum), Intbytes);

    asendmsg(req);

    freemsg(req);
}

void jia_waitcv(int cvnum) {
    jia_msg_t *req;
    int lockid;

    if (system_setting.hostc == 1)
        return;

    jia_assert(((cvnum >= 0) && (cvnum < Maxcvs)),
           "condv %d should range from 0 to %d", cvnum, Maxcvs - 1);

    req = newmsg();
    req->op = WAITCV;
    req->frompid = system_setting.jia_pid;
    req->topid = cvnum % system_setting.hostc;
    req->size = 0;
    appendmsg(req, ltos(cvnum), Intbytes);

    cvwait = 1;

    asendmsg(req);

    freemsg(req);
    while (cvwait)
        ;
}

/**
 * @brief pushstack -- push lock into lockstack
 *
 * @param lock lock id
 */
void pushstack(int lock) {
    stackptr++;
    jia_assert((stackptr < Maxstacksize), "Too many continuous ACQUIRES!");

    top.lockid = lock;
}

/**
 * @brief popstack -- pop the top lock in lockstack
 *
 * (save the unlucky one's write notice pairs (wtnts[i], from[i]) to the new top
 * one)
 *
 */
void popstack() {
    int wtnti;
    wtnt_t *wnptr;

    stackptr--;
    jia_assert((stackptr >= -1), "More unlocks than locks!");

    if (stackptr >= 0) {
        wnptr = lockstack[stackptr + 1].wtntp;
        while (wnptr != WNULL) {
            for (wtnti = 0; wtnti < wnptr->wtntc; wtnti++)
                savewtnt(top.wtntp, wnptr->wtnts[wtnti], wnptr->from[wtnti]);
            wnptr = wnptr->more;
        }
    }

    freewtntspace(lockstack[stackptr + 1].wtntp);
}

#else /* NULL_LIB */
void jia_lock(int lock) {}

void jia_unlock(int lock) {}

void jia_barrier() {}

void jia_wait() {}

void jia_setcv(int lock) {}

void jia_resetcv(int lock) {}

void jia_waitcv() {}

#endif /* NULL_LIB */