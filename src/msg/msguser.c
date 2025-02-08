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
#include "tools.h"

extern int nextmsg(char *buf, int len, int fromproc, int tag);
static int thesizeof(int op);
static void reduce(char *dest, char *source, int count, int op);

extern volatile int endofmsg;

/******************** Message Passing Part*****************/

int jia_recv(char *buf, int len, int fromproc, int tag) {
    int msgsize;

    /** init endofmsg and get msg */
    endofmsg = 0;
    msgsize = nextmsg(buf, len, fromproc, tag);

    return (msgsize);
}

void jia_send(char *buf, int len, int toproc, int tag) {
    jia_msg_t *req;
    int msgsize = len;
    char *msgptr = buf;
    int index;

    jia_assert(((toproc < system_setting.hostc) && (toproc >= 0)),
               "Incorrect message destination");

    /** step 1: init msg's metadata */
    index = freemsg_lock(&msg_buffer);
    req = &msg_buffer.buffer[index].msg;
    req->frompid = system_setting.jia_pid;
    req->topid = toproc;
    req->scope = tag;

    /** step 2: send msgs (MSGBODY and MSGTAIL) to other host*/
    while (msgsize > Maxmsgsize) {
        req->op = MSGBODY;
        req->size = 0;
        appendmsg(req, (unsigned char *)msgptr, Maxmsgsize);
        move_msg_to_outqueue(&msg_buffer, index, &outqueue);
        msgptr += Maxmsgsize;
        msgsize -= Maxmsgsize;
    }
    req->op = MSGTAIL;
    req->size = 0;
    appendmsg(req, (unsigned char *)msgptr, msgsize);
    move_msg_to_outqueue(&msg_buffer, index, &outqueue);

    /** step 3: free msg*/
    freemsg_unlock(&msg_buffer, index);
}

/*
 *  (no matter what root's jiapid is, its mypid in this tree is 0)
 *  (example for hostc=8), reduce every host's info into host 0
 *
 *  0                 1   2   3   level
 *  0--------|-(recv)-0---0---0
 *  1-(send)-|            |   |
 *                        |   |
 *  2--------|-(recv)-2---|   |
 *  3-(send)-|                |
 *                            |
 *  4--------|-(recv)-4---4---|
 *  5-(send)-|            |
 *                        |
 *  6--------|-(recv)-6---|
 *  7-(send)-|
 */
void jia_reduce(char *sendbuf, char *recvbuf, int count, int op, int root) {
    int j, hosts;
    int len, recvlen;
    int mypid, fromproc, toproc;
    char *tempbuf;

    jia_assert((root < system_setting.hostc) && (root >= 0),
               "Incorrect root in reduce");

    /** step 1: prepare tempbuf for reduce */
    len = count * thesizeof(op);
    memcpy(recvbuf, sendbuf, len);
    tempbuf = malloc(len);

    /** step 2: get its mypid in reduce tree */
    mypid = (system_setting.jia_pid - root + system_setting.hostc) %
            system_setting.hostc;

    /** step 3: start reduce recv and send */
    for (j = 0; (1 << j) < system_setting.hostc; j++) {
        if (mypid % (1 << j) == 0) {
            /** if host's pid is not in the next level, it will send its
             * reduce-info to its peer host, otherwise it will recv its peer
             * host's info*/
            if ((mypid % (1 << (j + 1))) != 0) {
                toproc = (mypid - (1 << j) + root) % system_setting.hostc;
                jia_send(recvbuf, len, toproc, REDUCE_TAG);
            } else if ((mypid + (1 << j)) < system_setting.hostc) {
                fromproc = (mypid + (1 << j) + root) % system_setting.hostc;
                recvlen = jia_recv(tempbuf, len, fromproc, REDUCE_TAG);
                jia_assert((len == recvlen), "Unmatched length in jia_reduce");

                reduce(recvbuf, tempbuf, count, op);
            }
        }
    }
    free(tempbuf);
}

void jia_bcast(char *buf, int len, int root) {
    jia_msg_t *req;
    int msgsize;
    char *msgptr;
    int index;

    if (system_setting.jia_pid == root) {
        msgsize = len;
        msgptr = buf;

        /** step 1: init msg's metadata */
        index = freemsg_lock(&msg_buffer);
        req = &msg_buffer.buffer[index].msg;
        req->frompid = system_setting.jia_pid;
        req->topid = system_setting.jia_pid;
        req->scope = BCAST_TAG;

        /** step 2: bsend msgs (MSGBODY and MSGTAIL) to other host*/
        while (msgsize > Maxmsgsize) {
            req->op = MSGBODY;
            req->size = 0;
            appendmsg(req, (unsigned char *)msgptr, Maxmsgsize);
            bsendmsg(req);
            msgptr += Maxmsgsize;
            msgsize -= Maxmsgsize;
        }
        req->op = MSGTAIL;
        req->size = 0;
        appendmsg(req, (unsigned char *)msgptr, msgsize);
        bsendmsg(req);

        /** step 3: free msg*/
        freemsg_unlock(&msg_buffer, index);
    }

    jia_recv(buf, len, MSG_PROC_ALL, BCAST_TAG);
}

/**
 * @brief thesizeof -- get corresponding size of the op type
 *
 * @param op specified macro
 * @return int
 */
static int thesizeof(int op) {
    int len;

    switch (op) {
    case SUM_INT:
    case MAX_INT:
    case MIN_INT:
        len = sizeof(int);
        break;
    case SUM_FLOAT:
    case MAX_FLOAT:
    case MIN_FLOAT:
        len = sizeof(float);
        break;
    case SUM_DOUBLE:
    case MAX_DOUBLE:
    case MIN_DOUBLE:
        len = sizeof(double);
        break;
    default:
        jia_assert(0, "Incorrect operation in jia_reduce");
    }
    return (len);
}

/**
 * @brief reduce - reduce msg to compute
 *
 * @param dest
 * @param source
 * @param count
 * @param op
 */
static void reduce(char *dest, char *source, int count, int op) {
    int k;

    switch (op) {
    case SUM_INT:
        for (k = 0; k < count; k++) {
            ((int *)dest)[k] += ((int *)source)[k];
        }
        break;
    case SUM_FLOAT:
        for (k = 0; k < count; k++) {
            ((float *)dest)[k] += ((float *)source)[k];
        }
        break;
    case SUM_DOUBLE:
        for (k = 0; k < count; k++) {
            ((double *)dest)[k] += ((double *)source)[k];
        }
        break;
    case MAX_INT:
        for (k = 0; k < count; k++) {
            ((int *)dest)[k] = MAX(((int *)source)[k], ((int *)dest)[k]);
        }
        break;
    case MAX_FLOAT:
        for (k = 0; k < count; k++) {
            ((float *)dest)[k] = MAX(((float *)source)[k], ((float *)dest)[k]);
        }
        break;
    case MAX_DOUBLE:
        for (k = 0; k < count; k++) {
            ((double *)dest)[k] =
                MAX(((double *)source)[k], ((double *)dest)[k]);
        }
        break;
    case MIN_INT:
        for (k = 0; k < count; k++) {
            ((int *)dest)[k] = MIN(((int *)source)[k], ((int *)dest)[k]);
        }
        break;
    case MIN_FLOAT:
        for (k = 0; k < count; k++) {
            ((float *)dest)[k] = MIN(((float *)source)[k], ((float *)dest)[k]);
        }
        break;
    case MIN_DOUBLE:
        for (k = 0; k < count; k++) {
            ((double *)dest)[k] =
                MIN(((double *)source)[k], ((double *)dest)[k]);
        }
        break;

    default:
        jia_assert(0, "Incorrect operation in jia_reduce");
    }
}

#else /* NULL_LIB */

#endif /* NULL_LIB */
