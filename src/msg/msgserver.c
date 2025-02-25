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
#include "msg.h"
#ifndef NULL_LIB
#include "tools.h"

extern volatile int recvwait;
extern jia_msg_t msgbuf[Maxmsgbufs]; /* message buffer */
extern unsigned long msgseqno;
extern msg_buffer_t msg_buffer;

void msgrecvserver(jia_msg_t *req) {
    int i = 0;
    int empty;

    /** step 1: find one free msgbuf whose op != ERRMSG*/
    msgseqno++;
    while ((msgbuf[i].op != ERRMSG) && (i < Maxmsgbufs))
        i++;

    /** step 2: memcpy msg to the free msgbuf*/
    if (i < Maxmsgbufs) {
        msgbuf[i].op = req->op;
        msgbuf[i].frompid = req->frompid;
        msgbuf[i].topid = req->topid;
        msgbuf[i].scope = req->scope; // This is tag
        msgbuf[i].seqno = msgseqno;
        msgbuf[i].size = req->size;
        memcpy(msgbuf[i].data, req->data, req->size);
        recvwait = 0;
    } else {
        local_assert(0, "Message Buffer Overflow!");
    }
}

/*
 *  (no matter what root's jiapid is, its mypid in this tree is 0)
 *  (example for hostc=8)
 *
 *  3   2   1   0   level
 *  0---0---0---0
 *  |   |   |---1
 *  |   |
 *  |   |---2---2
 *  |       |---3
 *  |
 *  |---4---4---4
 *      |   |---5
 *      |
 *      |---6---6
 *          |---7
 */
void bcastserver(jia_msg_t *msg) {
    int mypid, child1, child2;
    int rootlevel, root, level;
    slot_t *slot;

    int jia_pid = system_setting.jia_pid;
    int hostc = system_setting.hostc;

    /** step 1: get root and current level
     * (ensure current host's child1 and child2) */
    rootlevel = msg->temp;
    root = (rootlevel >> 16) & 0xffff;
    level = rootlevel & 0xffff;
    level--;

    /** step 2: mypid is current host's position in broadcast tree */
    mypid = ((jia_pid - root) >= 0) ? (jia_pid - root) : (jia_pid - root + hostc);
    child1 = mypid;
    child2 = mypid + (1 << level);

    /** step 3: broadcast msg to child1 and child2 */
    if (level == 0) {
        /* if level==0, msg must be handled and stop broadcast in the last level
         */
        msg->op -= BCAST;
    }
    msg->temp = ((root & 0xffff) << 16) | (level & 0xffff);
    msg->frompid = jia_pid;
    if (child2 < hostc) {
        msg->topid = (child2 + root) % hostc;
        move_msg_to_outqueue((slot_t *)msg, &outqueue); //jia_msg's addr is same to slot's addr
    }
    msg->topid = (child1 + root) % hostc;
    move_msg_to_outqueue((slot_t *)msg, &outqueue); //jia_msg's addr is same to slot's addr
}

#else /* NULL_LIB */

#endif /* NULL_LIB */
