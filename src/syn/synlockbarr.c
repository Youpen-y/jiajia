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
#include "msg.h"

/* syn */
extern jialock_t locks[Maxlocks + 1];
extern volatile int waitwait, cvwait, acqwait, barrwait;
extern int B_CAST;

/************Lock Part****************/

/**
 * @brief clearlocks() -- free the write notice space that used by lock
 *
 */
void clearlocks() {
    int i;

    for (i = system_setting.jia_pid; i < Maxlocks; i += system_setting.hostc) {
        freewtntspace(locks[i].wtntp);
    }
}

/**
 * @brief acquire() -- send ACQ msg to lock%hostc to acquire the lock
 *
 * @param lock
 *
 * acq msg data: | lock |
 */
void acquire(int lock) {
    jia_msg_t *req;
    int index;

    // req = newmsg();
    index = freemsg_lock(&msg_buffer);
    req = &msg_buffer.buffer[index].msg;
    req->op = ACQ;
    req->frompid = system_setting.jia_pid;
    req->topid = lock % system_setting.hostc; // it seems that lock was divided circularly
    req->scope = locks[lock].myscope;
    req->size = 0;
    appendmsg(req, ltos(lock), Intbytes);

    // asendmsg(req); // send the ACQ msg the lock owner
    // freemsg(req);

    move_msg_to_outqueue(&msg_buffer, index, &outqueue);
    freemsg_unlock(&msg_buffer, index);
    while (acqwait)
        ;
    log_info(3, "acquire lock!!!");
}

/**
 * @brief grantlock -- grant the acquire lock msg request
 *
 * @param lock
 * @param toproc
 * @param acqscope
 *
 * ACQGRANT msg data : | lock | addr1 | ... | addrn |
 */
void grantlock(int lock, int toproc, int acqscope) {
    jia_msg_t *grant;
    wtnt_t *wnptr;
    int index;

    index = freemsg_lock(&msg_buffer);
    grant = &msg_buffer.buffer[index].msg;
    grant->frompid = system_setting.jia_pid;
    grant->topid = toproc;
    grant->scope = locks[lock].scope;
    grant->size = 0;

    appendmsg(grant, ltos(lock), Intbytes);

    wnptr = locks[lock].wtntp;
    wnptr = appendlockwtnts(grant, wnptr, acqscope);
    while (wnptr != WNULL) { // current msg is full, but still wtnts  left
        grant->op = INVLD;
        // asendmsg(grant);
        move_msg_to_outqueue(&msg_buffer, index, &outqueue);
        grant->size = Intbytes;
        wnptr = appendlockwtnts(grant, wnptr, acqscope);
    }

    grant->op = ACQGRANT;

    move_msg_to_outqueue(&msg_buffer, index, &outqueue);
    freemsg_unlock(&msg_buffer, index);
}


/************Barrier Part****************/

/**
 * @brief grantbarr -- grant barr request msg
 *
 * @param lock
 *
 * grantbarr msg data: | lock(4bytes)  | maybe (addr (8bytes)) , from(4bytes)
 */
void grantbarr(int lock) {
    jia_msg_t *grant;
    wtnt_t *wnptr;
    int hosti, index;

    // grant = newmsg();
    index = freemsg_lock(&msg_buffer);
    grant = &msg_buffer.buffer[index].msg;

    grant->frompid = system_setting.jia_pid;
    grant->topid = system_setting.jia_pid;
    grant->scope = locks[lock].scope;
    grant->size = 0;

    appendmsg(grant, ltos(lock), Intbytes); // encapsulate the lock

    wnptr = locks[lock].wtntp; // get lock's write notice list's head
    wnptr = appendbarrwtnts(grant,
                            wnptr); // append lock's write notice contents(addr,
                                    // from) to msg if possible
    while (wnptr != WNULL) {
        grant->op = INVLD;
        broadcast(grant, index);
        grant->size = Intbytes; // lock
        wnptr = appendbarrwtnts(grant, wnptr);
    }
    grant->op = BARRGRANT;
    broadcast(grant, index);
    freemsg_unlock(&msg_buffer, index);
}

/**
 * @brief broadcast -- broadcast msg to all hosts
 *
 * @param msg msg that will be broadcast
 * @param index index of msg in msg_buffer
 */
void broadcast(jia_msg_t *msg, int index) {
    int hosti;

    if (B_CAST == OFF) {
        for (hosti = 0; hosti < system_setting.hostc; hosti++) {
            msg->topid = hosti;
            // asendmsg(msg);
            move_msg_to_outqueue(&msg_buffer, index, &outqueue);
        }
    } else {
        bsendmsg(msg);
    }
}