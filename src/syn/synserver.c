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

/* syn */
extern jiacv_t condvars[Maxcvs];
extern volatile int waitcounter;
extern volatile int waitwait, cvwait, acqwait, barrwait;
extern volatile int noclearlocks;
// lock array, according to the hosts allocate lock(eg.
// host0: 0, 2,... 62; host1 = 1, 3, ..., 63)
extern jialock_t locks[Maxlocks + 1];

extern int H_MIG;

/************Conditional Variable Part****************/

/**
 * @brief setcvserver -- setcv msg server
 *
 * @param req SETCV msg request
 */
void setcvserver(jia_msg_t *req) {
    int condv;
    int i;

    assert((req->op == SETCV) && (req->topid == system_setting.jia_pid),
           "Incorrect SETCV Message!");

    condv = (int)stol(req->data);
    assert((condv % system_setting.hostc == system_setting.jia_pid), "Incorrect home of condv!");

    condvars[condv].value = 1;

    if (condvars[condv].waitc > 0) {
        for (i = 0; i < condvars[condv].waitc; i++)
            grantcondv(condv, condvars[condv].waits[i]);
        condvars[condv].waitc = 0;
    }
}

/**
 * @brief resetcvserver -- resetcv msg server
 *
 * @param req resetcv msg
 */
void resetcvserver(jia_msg_t *req) {
    int condv;

    assert((req->op == RESETCV) && (req->topid == system_setting.jia_pid),
           "Incorrect RESETCV Message!");
    condv = (int)stol(req->data); // get the condition variable
    condvars[condv].value = 0;
}

/**
 * @brief waitcvserver -- if req->op
 *
 * @param req
 */
void waitcvserver(jia_msg_t *req) {
    int condv;
    int i;

    assert((req->op == WAITCV) && (req->topid == system_setting.jia_pid),
           "Incorrect WAITCV Message!");

    condv = (int)stol(req->data);

    condvars[condv].waits[condvars[condv].waitc] = req->frompid;
    condvars[condv].waitc++;

    if (condvars[condv].value == 1) {
        for (i = 0; i < condvars[condv].waitc; i++)
            grantcondv(condv, condvars[condv].waits[i]);
        condvars[condv].waitc = 0;
    }
}

/**
 * @brief cvgrantserver -- if msg req's op == CVGRANT and req->topid == system_setting.jia_pid,
 * set cvwait = 0
 *
 * @param req jia msg
 */
void cvgrantserver(jia_msg_t *req) {
    int condv;

    assert((req->op == CVGRANT) && (req->topid == system_setting.jia_pid),
           "Incorrect CVGRANT Message!");
    condv = (int)stol(req->data);

    // ...
    cvwait = 0;
}


/************Wait Part****************/

/**
 * @brief waitserver -- WAIT msg request server
 *
 * @param req WAIT msg request
 */
void waitserver(jia_msg_t *req) {
    jia_msg_t *grant;
    int i;

    assert((req->op == WAIT) && (req->topid == system_setting.jia_pid),
           "Incorrect WAIT Message!");

    waitcounter++;

    if (waitcounter == system_setting.hostc) {
        grant = newmsg();
        waitcounter = 0;
        grant->frompid = system_setting.jia_pid;
        grant->size = 0;
        grant->op = WAITGRANT;
        broadcast(grant);
        freemsg(grant);
    }
}

/**
 * @brief waitgrantserver -- waitgrant msg server
 *
 * @param req WAITGRANT msg request
 */
void waitgrantserver(jia_msg_t *req) {
    assert((req->op == WAITGRANT) && (req->topid == system_setting.jia_pid),
           "Incorrect WAITGRANT Message!");

    waitwait = 0;
}

/************Lock Part****************/

/**
 * @brief invserver -- INVLD msg server
 *
 * @param req INVID msg
 */
void invserver(jia_msg_t *req) {
    assert((req->op == INVLD) && (req->topid == system_setting.jia_pid),
           "Incorrect INVLD Message!");

    invalidate(req);
}

/**
 * @brief acqserver -- ACQ msg server
 *
 * @param req ACQ msg
 *
 * ACQ msg data : | lock |
 */
void acqserver(jia_msg_t *req) {
    int lock;
    int wtnti;

    assert((req->op == ACQ) && (req->topid == system_setting.jia_pid),
           "Incorrect ACQ message!");

    lock = (int)stol(req->data); // get the lock
    assert((lock % system_setting.hostc == system_setting.jia_pid), "Incorrect home of lock!");

    locks[lock].acqs[locks[lock].acqc] = req->frompid;
    locks[lock].acqscope[locks[lock].acqc] = req->scope;
    locks[lock].acqc++;

    if (locks[lock].acqc == 1)
        grantlock(lock, locks[lock].acqs[0], locks[lock].acqscope[0]);
}

/**
 * @brief relserver -- REL msg server
 *
 * @param req request msg REL
 *
 * REL msg data : | lock | addr | ... | addr
 */
void relserver(jia_msg_t *req) {
    int lock;
    int acqi;

    assert((req->op == REL) && (req->topid == system_setting.jia_pid),
           "Incorrect REL Message!");

    lock = (int)stol(req->data); // get the lock
    assert((lock % system_setting.hostc == system_setting.jia_pid), "Incorrect home of lock!");
    assert((req->frompid == locks[lock].acqs[0]),
           "This should not have happened! 6");

    if (req->scope > locks[hidelock].myscope)
        noclearlocks = 1;

    recordwtnts(req);
    locks[lock].scope++;

    for (acqi = 0; acqi < (locks[lock].acqc - 1); acqi++) {
        locks[lock].acqs[acqi] = locks[lock].acqs[acqi + 1];
        locks[lock].acqscope[acqi] = locks[lock].acqscope[acqi + 1];
    }
    locks[lock].acqc--;

    if (locks[lock].acqc > 0)
        grantlock(lock, locks[lock].acqs[0], locks[lock].acqscope[0]);
}

/**
 * @brief acqgrantserver -- ACQGRANT msg server
 *
 * @param req ACQGRANT msg
 */
void acqgrantserver(jia_msg_t *req) {
    int lock;

    assert((req->op == ACQGRANT) && (req->topid == system_setting.jia_pid),
           "Incorrect ACQGRANT Message!");
    invalidate(req);

    lock = (int)stol(req->data);
    locks[lock].myscope = req->scope;
    acqwait = 0;
}


/************Barrier Part****************/

/**
 * @brief barrserver -- barr msg server
 *
 * @param req barr msg
 * barr msg data: | lock (4bytes) |
 */
void barrserver(jia_msg_t *req) {
    VERBOSE_LOG(3, "host %d is running in barrserver\n", jiapid);
    int lock;

    assert((req->op == BARR) && (req->topid == system_setting.jia_pid),
           "Incorrect BARR Message!");

    lock = (int)stol(req->data);

    assert((lock % system_setting.hostc == system_setting.jia_pid), "Incorrect home of lock!");
    assert((lock == hidelock), "This should not have happened! 8");

    recordwtnts(req); // record write notices in msg barr's data

    locks[lock].acqc++;

#ifdef DEBUG
    VERBOSE_LOG(3, "barrier count=%d, from host %d\n", locks[lock].acqc, req->frompid);
#endif
    VERBOSE_LOG(3, "locks[%d].acqc = %d\n", lock, locks[lock].acqc);
    if (locks[lock].acqc == system_setting.hostc) {
        locks[lock].scope++;
        grantbarr(lock);
        locks[lock].acqc = 0;
        freewtntspace(locks[lock].wtntp);
    }
    VERBOSE_LOG(3, "host %d is out of barrserver\n", jiapid);
}

/**
 * @brief barrgrantserver -- barrgrant msg server
 *
 * @param req BARRGRANT msg
 */
void barrgrantserver(jia_msg_t *req) {
    int lock;

    assert((req->op == BARRGRANT) && (req->topid == system_setting.jia_pid),
           "Incorrect BARRGRANT Message!");

    if (noclearlocks == 0)
        clearlocks();
    invalidate(req);

    if (H_MIG == ON) {
        migarrangehome();
    }

    lock = (int)stol(req->data);
    locks[lock].myscope = req->scope;
    barrwait = 0;
    noclearlocks = 0;
}


/************Wtnt Part****************/

/**
 * @brief wtntserver -- msg wtnt server
 *
 * @param req
 *
 * wtnt msg: | lock | addr | addr |...|
 */
void wtntserver(jia_msg_t *req) {
    int lock;

    assert((req->op == WTNT) && (req->topid == system_setting.jia_pid),
           "Incorrect WTNT Message!");

    lock = (int)stol(req->data);
    assert((lock % system_setting.hostc == system_setting.jia_pid), "Incorrect home of lock!");

    recordwtnts(req);
}