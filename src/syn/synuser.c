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
 **********************************************************************/

#ifndef NULL_LIB
#include "comm.h"
#include "global.h"
#include "init.h"
#include "jia.h"
#include "mem.h"
#include "syn.h"
#include "tools.h"
#include "setting.h"
#include "stat.h"
#include "msg.h"
#include <stdatomic.h>

#ifdef DOSTAT
extern int statflag;
extern jiastat_t jiastat;
#endif

/* syn */
extern float caltime, starttime, endtime;
extern int H_MIG, LOAD_BAL, W_VEC;
extern _Atomic volatile int waitwait, cvwait, acqwait, barrwait;
extern int stackptr;
extern jiastack_t lockstack[Maxstacksize]; // lock stack
extern void pushstack(int lock);
extern void popstack();
extern void endinterval(int synop);
extern void startinterval(int synop);
extern void sendwtnts(int operation);
extern void migcheckcache();

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

    //acqwait = 1;
    atomic_store(&acqwait, 1);
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
 * @brief jia_unlock -- unlock the lock
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

    log_info(3, "Enter jia barrier: %d", jiastat.barrcnt);
    endinterval(BARR);

    if (H_MIG == ON) {
        migcheckcache();
    }

    //barrwait = 1;
    atomic_store(&barrwait, 1);
    sendwtnts(BARR);
    freewtntspace(top.wtntp);
    while (atomic_load(&barrwait))
        ;
    if ((H_MIG == ON) && (W_VEC == ON)) {
        jia_wait();
    }
    startinterval(BARR);
    if (LOAD_BAL == ON)
        starttime = jia_clock();

#ifdef DOSTAT
    if (statflag == 1) {
        jiastat.barrtime += get_usecs() - begin;
        jiastat.kernelflag = 0;
    }
#endif
    log_info(3, "jia_barrier completed\n");
}


/************Wait Part****************/

/**
 * @brief jia_wait -- send WAIT msg to master and wait for WAITGRANT reply to be
 * served
 *
 */
void jia_wait() {
#ifdef DOSTAT
    register unsigned int begin = get_usecs();
#endif
    jia_msg_t *req;
    slot_t* slot;

    if (system_setting.hostc == 1)
        return;

    if (LOAD_BAL == ON) {
        endtime = jia_clock();
        caltime += (endtime - starttime);
    }

    // construct WAIT msg
    slot = freemsg_lock(&msg_buffer);
    req = &(slot->msg);
    req->frompid = system_setting.jia_pid;
    req->topid = 0;
    req->op = WAIT;
    req->size = 0;

    atomic_store(&waitwait, 1);

    // send WAIT msg
    move_msg_to_outqueue(slot, &outqueue);
    freemsg_unlock(slot);

    // busywaiting until waitwait is clear by waitgrantserver
    while (atomic_load(&waitwait))
        ;

#ifdef DOSTAT
    jiastat.waittime += get_usecs() - begin;
#endif

    if (LOAD_BAL == ON)
        starttime = jia_clock();
}


/************Conditional Variable Part****************/

/**
 * @brief jia_setcv -- send SETCV msg to the holder of the cvnum
 */
void jia_setcv(int cvnum) {
    jia_msg_t *req;
    slot_t* slot;

    if (system_setting.hostc == 1)
        return;

    jia_assert(((cvnum >= 0) && (cvnum < Maxcvs)),
           "condv %d should range from 0 to %d", cvnum, Maxcvs - 1);

    slot = freemsg_lock(&msg_buffer);
    req = &(slot->msg);
    req->op = SETCV;
    req->frompid = system_setting.jia_pid;
    req->topid = cvnum % system_setting.hostc;
    req->size = 0;
    appendmsg(req, ltos(cvnum), Intbytes);

    move_msg_to_outqueue(slot, &outqueue);
    freemsg_unlock(slot);
}

void jia_resetcv(int cvnum) {
    jia_msg_t *req;
    slot_t* slot;

    if (system_setting.hostc == 1)
        return;

    jia_assert(((cvnum >= 0) && (cvnum < Maxcvs)),
           "condv %d should range from 0 to %d", cvnum, Maxcvs - 1);

    slot = freemsg_lock(&msg_buffer);
    req = &(slot->msg);
    req->op = RESETCV;
    req->frompid = system_setting.jia_pid;
    req->topid = cvnum % system_setting.hostc;
    req->size = 0;
    appendmsg(req, ltos(cvnum), Intbytes);

    move_msg_to_outqueue(slot, &outqueue);
    freemsg_unlock(slot);
}

void jia_waitcv(int cvnum) {
    jia_msg_t *req;
    int lockid;
    slot_t* slot;

    if (system_setting.hostc == 1)
        return;

    jia_assert(((cvnum >= 0) && (cvnum < Maxcvs)),
           "condv %d should range from 0 to %d", cvnum, Maxcvs - 1);

    slot = freemsg_lock(&msg_buffer);
    req = &(slot->msg);
    req->op = WAITCV;
    req->frompid = system_setting.jia_pid;
    req->topid = cvnum % system_setting.hostc;
    req->size = 0;
    appendmsg(req, ltos(cvnum), Intbytes);

    atomic_store(&cvwait, 1);

    move_msg_to_outqueue(slot, &outqueue);
    freemsg_unlock(slot);
    while (atomic_load(&cvwait))
        ;
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