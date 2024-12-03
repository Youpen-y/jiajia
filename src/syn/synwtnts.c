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
#include "msg.h"
#include "setting.h"
#include "stat.h"
#include "syn.h"
#include "tools.h"
#include "utils.h"

/* syn */
extern jiapage_t page[Maxmempages];
// lock array, according to the hosts allocate lock(eg.
// host0: 0, 2,... 62; host1 = 1, 3, ..., 63)
extern jialock_t locks[Maxlocks + 1];
extern jiastack_t lockstack[Maxstacksize];
extern int stackptr;

static wtnt_t *appendstackwtnts(jia_msg_t *msg, wtnt_t *ptr);

/**
 * @brief savewtnt -- save write notice about the page of the addr to ptr
 *
 * @param ptr lockstack[stackptr]'s wtnt_t pointer
 * @param addr the original address of cached page
 * @param frompid
 */
void savewtnt(wtnt_t *ptr, address_t addr, int frompid) {
    int wtnti;
    int exist;
    wtnt_t *wnptr;

    exist = 0;
    wnptr = ptr;

    /**
     * step 1:
     * check if there is already an address same to the addr in the WTNT list.
     */
    while ((exist == 0) && (wnptr != WNULL)) {
        wtnti = 0;
        while ((wtnti < wnptr->wtntc) &&
               (((unsigned long)addr / Pagesize) !=
                ((unsigned long)wnptr->wtnts[wtnti] / Pagesize))) {
            wtnti++;
        }

        // Traversing wtnt list to find addr
        if (wtnti >= wnptr->wtntc)
            wnptr = wnptr->more;
        else
            exist = 1;
    }

    /**
     * step 2:
     * existed: Change its frompid.
     * not existed: newwtnt() && record new addr
     */
    if (exist == 0) {
        wnptr = ptr;
        while (wnptr->wtntc >= Maxwtnts) {
            if (wnptr->more == WNULL)
                wnptr->more = newwtnt();
            wnptr = wnptr->more;
        }
        wnptr->wtnts[wnptr->wtntc] = addr;
        wnptr->from[wnptr->wtntc] = frompid;
        wnptr->wtntc++;
    } else {
        if (ptr == locks[hidelock].wtntp) { /*barrier*/
            wnptr->from[wtnti] = Maxhosts;
        } else {
            wnptr->from[wtnti] = frompid; /*lock or stack*/
        }
    }
}

/**
 * @brief recordwtnts -- record write notices in msg req's data
 *
 * @param req request msg
 */
void recordwtnts(jia_msg_t *req) {
    int lock;
    int datai;

    lock = (int)stol(req->data); // get the lock
    if (lock != hidelock) {      /*lock*/
        for (datai = Intbytes; datai < req->size;
             datai += sizeof(unsigned char *))
            savewtnt(locks[lock].wtntp, (address_t)stol(req->data + datai),
                     locks[lock].scope);
    } else { /*barrier*/
        for (datai = Intbytes; datai < req->size;
             datai += sizeof(unsigned char *))
            savewtnt(locks[lock].wtntp, (address_t)stol(req->data + datai),
                     req->frompid);
    }
}

/**
 * @brief sendwtnts() -- send wtnts
 *
 * @param operation BARR or REL
 *
 * BARR or REL message's data : | top.lockid(4bytes) | addr1 | ... | addrn |
 */
void sendwtnts(int operation) {
    int wtnti, index;
    jia_msg_t *req;
    wtnt_t *wnptr; // write notice pointer

    log_info(3, "Enter sendwtnts!");

    // req = newmsg();
    index = freemsg_lock(&msg_buffer);
    req = &(msg_buffer.buffer[index].msg);
    req->frompid = system_setting.jia_pid;
    req->topid = top.lockid % system_setting.hostc;
    req->size = 0;
    req->scope = (operation == BARR) ? locks[hidelock].myscope
                                    : locks[top.lockid].myscope;
    appendmsg(req, ltos(top.lockid),
              Intbytes); // note here, after ltos transformation(8bytes),
                         // truncation here

    wnptr = top.wtntp;
    wnptr = appendstackwtnts(req, wnptr);

    /**
     * send wtnt(,Packing for delivery)
     * in the end send msg in which op==REL to finish this wtnt msg stream
     */
    while (wnptr != WNULL) {
        req->op = WTNT;
        // asendmsg(req);
        move_msg_to_outqueue(&msg_buffer, index, &outqueue);
        req->size = Intbytes; // TODO: Need to check
        wnptr = appendstackwtnts(req, wnptr);
    }
    req->op = operation;
    // asendmsg(req);
    // freemsg(req);
    move_msg_to_outqueue(&msg_buffer, index, &outqueue);
    freemsg_unlock(&msg_buffer, index);

    log_info(3, "Out of sendwtnts!");
}

/**
 * @brief appendstackwtnts -- append wtnt_t wtnts(i.e addr) to msg as many as
 * possible
 *
 * @param msg msg that include ptr->wtnts
 * @param ptr write notice pointer
 * @return wtnt_t*
 */
wtnt_t *appendstackwtnts(jia_msg_t *msg, wtnt_t *ptr) {
    int full; // flag indicate that msg full or not
    wtnt_t *wnptr;

    full = 0;
    wnptr = ptr;
    while ((wnptr != WNULL) && (full == 0)) {
        // if ((msg->size+(wnptr->wtntc*Intbytes)) < Maxmsgsize) {
        if ((msg->size + (wnptr->wtntc * (sizeof(unsigned char *)))) <
            Maxmsgsize) {
            // appendmsg(msg, wnptr->wtnts, (wnptr->wtntc*Intbytes));
            appendmsg(msg, wnptr->wtnts,
                      (wnptr->wtntc) * (sizeof(unsigned char *)));
            wnptr = wnptr->more;
        } else {
            full = 1;
        }
    }
    return (wnptr);
}

/**
 * @brief appendlockwtnts -- append lock's wtnts to msg
 *
 * @param msg
 * @param ptr
 * @param acqscope
 * @return wtnt_t*
 */
wtnt_t *appendlockwtnts(jia_msg_t *msg, wtnt_t *ptr, int acqscope) {
    int wtnti;
    int full;
    wtnt_t *wnptr;

    full = 0;
    wnptr = ptr;

    /* wnptr is not NULL and a msg is not full */
    while ((wnptr != WNULL) && (full == 0)) {
        if ((msg->size + (wnptr->wtntc * (sizeof(unsigned char *)))) <
            Maxmsgsize) {
            for (wtnti = 0; wtnti < wnptr->wtntc; wtnti++) 
                if ((wnptr->from[wtnti] > acqscope) &&
                    // if homehost(wnptr->wtnts[wtnti]) == msg->topid), not
                    // send(remote host is the owner of this copy)
                    (homehost(wnptr->wtnts[wtnti]) != msg->topid))
                    appendmsg(msg, ltos(wnptr->wtnts[wtnti]),
                              sizeof(unsigned char *));
            wnptr = wnptr->more;
        } else {
            full = 1;
        }
    }
    return (wnptr);
}

/**
 * @brief appendbarrwtnts -- append wtnt_t ptr's pair (wtnts, from) to msg
 *
 * @param msg grantbarr msg
 * @param ptr lock's write notice's pointer
 * @return wtnt_t*
 */
wtnt_t *appendbarrwtnts(jia_msg_t *msg, wtnt_t *ptr) {
    int wtnti;
    int full;
    wtnt_t *wnptr;

    full = 0;
    wnptr = ptr;
    while ((wnptr != WNULL) && (full == 0)) {
        // if ((msg->size+(wnptr->wtntc*Intbytes*2))<Maxmsgsize){
        if ((msg->size + (wnptr->wtntc *
                          (Intbytes + sizeof(unsigned char *)))) < Maxmsgsize) {
            for (wtnti = 0; wtnti < wnptr->wtntc; wtnti++) {
                // appendmsg(msg,ltos(wnptr->wtnts[wtnti]),Intbytes);
                appendmsg(msg, ltos(wnptr->wtnts[wtnti]),
                          sizeof(unsigned char *));
                appendmsg(msg, ltos(wnptr->from[wtnti]), Intbytes);
            }
            wnptr = wnptr->more;
        } else {
            full = 1;
        }
    }
    return (wnptr);
}