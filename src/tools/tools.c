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

#include <string.h>
#ifndef NULL_LIB
#include "tools.h"
#include "comm.h"
#include "global.h"
#include "init.h"
#include "mem.h"
#include "msg.h"
#include "setting.h"
#include "stat.h"
#include "syn.h"
#include <bits/types/clockid_t.h>
#include <libgen.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

extern jiastack_t lockstack[Maxstacksize];
extern int totalhome;
extern jiapage_t page[Maxmempages];
extern jiahome_t home[Homepages];
extern unsigned long globaladdr;
extern int firsttime;
extern float caltime;

FILE *logfile = NULL;
char errstr[Linesize]; /* buffer for error info */

jia_msg_t assertmsg;

/* optimization techniques flag */
int H_MIG = OFF;  // H_MIG: home migration flag(ON/OFF)
int AD_WD = OFF;  // AD_WD: adaptive write detection flag(ON/OFF)
int B_CAST = OFF; // B_CAST: broadcast barrier messages method either one-by-one
                  // or tree structure broadcast
int LOAD_BAL = OFF; // LOAD_BAL: load balancing flag(ON/OFF)
int W_VEC = OFF;    // W_VEC: write vector flag(ON/OFF)
int RDMA_SUPPORT = OFF;

// default prefetch optimization technique setting
struct prefetech_opt_t prefetch_optimization = {
    .base = {
        .flag = false,
        .name = "prefetch"
    },
    .prefetch_pages = 0,
    .max_checking_pages = 0
};

/************ other tools ****************/

/* initial setting that default optimization method is OFF */
void inittools() {
    H_MIG = OFF;
    AD_WD = OFF;
    B_CAST = OFF;
    LOAD_BAL = OFF;
    W_VEC = OFF;
    RDMA_SUPPORT = OFF; // TODO: RDMA_SUPPORT

    /** prefetch optimization */
    prefetch_optimization.base.flag = system_setting.prefetch_flag;
    prefetch_optimization.prefetch_pages = system_setting.prefetch_pages;
    prefetch_optimization.max_checking_pages = system_setting.max_checking_pages;
}

/**
 * @brief newtwin() -- alloc a Pagesize space for a page's copy
 *
 * @param twin address of the new allocated space
 */
void newtwin(address_t *twin) {
    if (*twin == ((address_t)NULL))
        *twin = (address_t)valloc(Pagesize);
    jia_assert(((*twin) != (address_t)NULL), "Cannot allocate twin space!");
}

/**
 * @brief freetwin() -- free the space pointed by twin
 *
 * @param twin address of memory that will be free
 */
void freetwin(address_t *twin) {
#ifndef RESERVE_TWIN_SPACE
    free(*twin);
    *twin = (address_t)NULL;
#endif
}

/**
 * @brief apppendmsg() -- append message with len bytes from str
 *
 * @param msg original message
 * @param str appended source
 * @param len number of bytes
 *
 */
void appendmsg(jia_msg_t *msg, unsigned char *str, int len) {
    jia_assert(((msg->size + len) <= Maxmsgsize), "Message too large");
    memcpy(msg->data + msg->size, str, len);
    msg->size += len;
}

/**
 * @brief newwtnt() -- allocate a space for a new wtnt_t object
 *
 * @return wtnt_t* return a pointer to the allocated memory on success, NULL is
 * returned on error and set errno
 *
 */
wtnt_t *newwtnt() {
    wtnt_t *wnptr;

#ifdef SOLARIS
    wnptr = memalign((size_t)Pagesize, (size_t)sizeof(wtnt_t));
#else  /* SOLARIS */
    wnptr = valloc((size_t)sizeof(wtnt_t));
#endif /* SOLARIS */
    jia_assert((wnptr != WNULL), "Can not allocate space for write notices!");
    wnptr->more = WNULL;
    wnptr->wtntc = 0;
    return (wnptr);
}

/**
 * @brief freewtntspace() -- free the wtnt list exclude the node pointed by ptr
 * (list head)
 *
 * @param ptr list head
 */
void freewtntspace(wtnt_t *ptr) {
    wtnt_t *last, *wtntp;

    wtntp = ptr->more;
    while (wtntp != WNULL) {
        last = wtntp;
        wtntp = wtntp->more;
        free(last);
    }
    ptr->wtntc = 0;
    ptr->more = WNULL;
}

void free_system_resources() {
    free_msg_buffer(&msg_buffer);  // free msg buffer
    free_msg_queue(&outqueue);     // free outqueue
    free_msg_queue(&inqueue);      // free inqueue
    free_setting(&system_setting); // free system setting
    // ...
}

/************ exit ****************/

/**
 * @brief local_assert -- assert the condition locally, if cond is false, print
 * the amsg and exit
 *
 * @param cond condition
 * @param amsg assert message
 */
void local_assert(int cond, char *format, ...) {
    if (!cond) {
        // print error message
        fprintf(stderr, "Assert0 error from host %d ---\n",
                system_setting.jia_pid);
        va_list args;
        va_start(args, format);
        vfprintf(stderr, format, args);
        va_end(args);

        // print Unix error; fflush&&exit
        perror("Unix Error");
        fflush(stderr);
        fflush(stdout);
        free_system_resources();
        exit(-1);
    }
}

/**
 * @brief jia_assert -- judge the cond condition and send assert message
 * If cond is falut, broadcast JIAEXIT msg
 *
 * @param cond conditons
 * @param amsg assert error message
 */
void jia_assert(int cond, char *format, ...) {
    int hosti;
    int index;
    jia_msg_t *assert_msg;

    if (!cond) { // if condition is false then send JIAEXIT msg to all hosts
        // init assertmsg
        log_err("enter jia_assert!!!");

        index = freemsg_lock(&msg_buffer);
        assert_msg = &msg_buffer.buffer[index].msg;

        va_list args;
        va_start(args, format);
        vsprintf((char *)assert_msg->data, format, args);
        log_info(3, "Errmsg: %s", assert_msg->data);
        va_end(args);
        assert_msg->op = JIAEXIT;
        assert_msg->frompid = system_setting.jia_pid;
        assert_msg->size = strlen((char *)assert_msg->data) + 1;

        // asend message
        for (hosti = 0; hosti < system_setting.hostc; hosti++) {
            if (hosti != system_setting.jia_pid) {
                assert_msg->topid = hosti;
                move_msg_to_outqueue(&msg_buffer, index, &outqueue);
                // asendmsg(&assertmsg);
            }
        }
        assertmsg.topid =
            system_setting.jia_pid; // self send JIAEXIT msg in the last
        // asendmsg(&assertmsg);
        move_msg_to_outqueue(&msg_buffer, index, &outqueue);
        freemsg_unlock(&msg_buffer, index);
        exit(-1);
    }
}

/**
 * @brief jiaexitserver -- output error message and exit
 *
 * @param req msg that will be printed
 */
void jiaexitserver(jia_msg_t *req) {
    log_err("Assert error from host %d --- %s\n", req->frompid,
            (char *)req->data);
    fflush(stderr);
    fflush(stdout);
    free_system_resources();
    exit(-1);
}

/**
 * @brief jia_error -- output str with possible format
 *
 * @param str string to output
 * @param ... variable arguments
 */
void jia_error(char *str, ...) {
    va_list ap;
    va_start(ap, str);
    vsprintf(errstr, str, ap);
    va_end(ap);
    jia_assert(0, errstr);
}

/************ print ****************/

int open_logfile(char *filename, int argc, char **argv) {
    char name[30];
    if (system_setting.jia_pid == 0) {
        logfile = fopen(filename, "w+");
    } else {
        sprintf(name, "./jianode/%s/%s", basename(argv[0]), filename);
        logfile = fopen(name, "w+");
    }
    if (logfile == NULL) {
        printf("Cannot open logfile %s\n", filename);
        return -1;
    }
    return 0;
}

char *op2name(int op) {
    switch (op) {
    case DIFF:
        return "DIFF";
    case DIFFGRANT:
        return "DIFFGRANT";
    case GETP:
        return "GETP";
    case GETPGRANT:
        return "GETPGRANT";
    case ACQ:
        return "ACQ";
    case ACQGRANT:
        return "ACQGRANT";
    case INVLD:
        return "INVALID";
    case BARR:
        return "BARR";
    case BARRGRANT:
        return "BARRGRANT";
    case REL:
        return "REL";
    case WTNT:
        return "WTNT";
    case JIAEXIT:
        return "JIAEXIT";
    case WAIT:
        return "WAIT";
    case WAITGRANT:
        return "WAITGRANT";
    case STAT:
        return "STAT";
    case STATGRANT:
        return "STATGRANT";
    case SETCV:
        return "SETCV";
    case RESETCV:
        return "RESETCV";
    case WAITCV:
        return "WAITCV";
    case CVGRANT:
        return "CVGRANT";
    case MSGBODY:
    case MSGTAIL:
        return "MSG";
    case LOADREQ:
        return "LOADREQ";
    case LOADGRANT:
        return "LOADGRANT";

    default:
        return "NULL";
    }
}

/**
 * @brief printmsg() -- print message pointed by msg (if verbose >= 3)
 * @param msg msg that will be printed
 */
void printmsg(jia_msg_t *msg) {

    if (verbose_out >= 3) {
        SPACE(1);
        VERBOSE_LOG(3, "********Print message********\n");
        SPACE(1);
        switch (msg->op) {
        case DIFF:
            VERBOSE_LOG(3, "msg.op      = DIFF     \n");
            SPACE(1);
            VERBOSE_LOG(3, "msg.frompid = %d\n", msg->frompid);
            SPACE(1);
            VERBOSE_LOG(3, "msg.topid   = %d\n", msg->topid);
            SPACE(1);
            VERBOSE_LOG(3, "msg.temp    = %d\n", msg->temp);
            SPACE(1);
            VERBOSE_LOG(3, "msg.seqno   = %d\n", msg->seqno);
            SPACE(1);
            VERBOSE_LOG(3, "msg.index   = %d\n", msg->index);
            SPACE(1);
            VERBOSE_LOG(3, "msg.size    = %d\n", msg->size);
            SPACE(1);
            VERBOSE_LOG(3, "msg.data(addr) = %lu\n",
                        stol(msg->data)); // addr(8bytes)
            SPACE(1);
            VERBOSE_LOG(3, "msg.data(totalsize)    = %d\n",
                        bytestoi(msg->data + 8));
            SPACE(1);
            VERBOSE_LOG(3, "msg.data(start|count)    = %d\n",
                        bytestoi(msg->data + 12));
            break;
        case DIFFGRANT:
            VERBOSE_LOG(3, "msg.op      = DIFFGRANT\n");
            SPACE(1);
            VERBOSE_LOG(3, "msg.frompid = %d\n", msg->frompid);
            SPACE(1);
            VERBOSE_LOG(3, "msg.topid   = %d\n", msg->topid);
            SPACE(1);
            VERBOSE_LOG(3, "msg.temp     =%d\n", msg->temp);
            SPACE(1);
            VERBOSE_LOG(3, "msg.seqno   = %d\n", msg->seqno);
            SPACE(1);
            VERBOSE_LOG(3, "msg.index   = %d\n", msg->index);
            SPACE(1);
            VERBOSE_LOG(3, "msg.size    = %d\n", msg->size);
            break;
        case GETP:
            VERBOSE_LOG(3, "msg.op      = GETP     \n");
            SPACE(1);
            VERBOSE_LOG(3, "msg.frompid = %d\n", msg->frompid);
            SPACE(1);
            VERBOSE_LOG(3, "msg.topid   = %d\n", msg->topid);
            SPACE(1);
            VERBOSE_LOG(3, "msg.temp     =%d\n", msg->temp);
            SPACE(1);
            VERBOSE_LOG(3, "msg.seqno   = %d\n", msg->seqno);
            SPACE(1);
            VERBOSE_LOG(3, "msg.index   = %d\n", msg->index);
            SPACE(1);
            VERBOSE_LOG(3, "msg.size    = %d\n", msg->size);
            SPACE(1);
            VERBOSE_LOG(3, "msg.data    = %lu\n", stol(msg->data));
            break;
        case GETPGRANT:
            VERBOSE_LOG(3, "msg.op      = GETPGRANT\n");
            SPACE(1);
            VERBOSE_LOG(3, "msg.frompid = %d\n", msg->frompid);
            SPACE(1);
            VERBOSE_LOG(3, "msg.topid   = %d\n", msg->topid);
            SPACE(1);
            VERBOSE_LOG(3, "msg.temp     =%d\n", msg->temp);
            SPACE(1);
            VERBOSE_LOG(3, "msg.seqno   = %d\n", msg->seqno);
            SPACE(1);
            VERBOSE_LOG(3, "msg.index   = %d\n", msg->index);
            SPACE(1);
            VERBOSE_LOG(3, "msg.size    = %d\n", msg->size);
            SPACE(1);
            VERBOSE_LOG(3, "msg.data    = %lu\n", stol(msg->data));
            SPACE(1);
            VERBOSE_LOG(3, "msg.data    = %lu\n", stol(msg->data + 8));
            break;
        case ACQ:
            VERBOSE_LOG(3, "msg.op      = ACQ      \n");
            SPACE(1);
            VERBOSE_LOG(3, "msg.frompid = %d\n", msg->frompid);
            SPACE(1);
            VERBOSE_LOG(3, "msg.topid   = %d\n", msg->topid);
            SPACE(1);
            VERBOSE_LOG(3, "msg.temp     =%d\n", msg->temp);
            SPACE(1);
            VERBOSE_LOG(3, "msg.seqno   = %d\n", msg->seqno);
            SPACE(1);
            VERBOSE_LOG(3, "msg.index   = %d\n", msg->index);
            SPACE(1);
            VERBOSE_LOG(3, "msg.size    = %d\n", msg->size);
            SPACE(1);
            VERBOSE_LOG(3, "msg.data    = %d\n", bytestoi(msg->data));
            break;
        case ACQGRANT:
            VERBOSE_LOG(3, "msg.op      = ACQGRANT \n");
            SPACE(1);
            VERBOSE_LOG(3, "msg.frompid = %d\n", msg->frompid);
            SPACE(1);
            VERBOSE_LOG(3, "msg.topid   = %d\n", msg->topid);
            SPACE(1);
            VERBOSE_LOG(3, "msg.temp     =%d\n", msg->temp);
            SPACE(1);
            VERBOSE_LOG(3, "msg.seqno   = %d\n", msg->seqno);
            SPACE(1);
            VERBOSE_LOG(3, "msg.index   = %d\n", msg->index);
            SPACE(1);
            VERBOSE_LOG(3, "msg.size    = %d\n", msg->size);
            SPACE(1);
            VERBOSE_LOG(3, "msg.data    = %d\n", bytestoi(msg->data));
            SPACE(1);
            VERBOSE_LOG(3, "msg.data    = %lu\n", stol(msg->data + 4));
            SPACE(1);
            VERBOSE_LOG(3, "msg.data    = %lu\n", stol(msg->data + 12));
            break;
        case INVLD:
            VERBOSE_LOG(3, "msg.op      = INVLD    \n");
            SPACE(1);
            VERBOSE_LOG(3, "msg.frompid = %d\n", msg->frompid);
            SPACE(1);
            VERBOSE_LOG(3, "msg.topid   = %d\n", msg->topid);
            SPACE(1);
            VERBOSE_LOG(3, "msg.temp     =%d\n", msg->temp);
            SPACE(1);
            VERBOSE_LOG(3, "msg.seqno   = %d\n", msg->seqno);
            SPACE(1);
            VERBOSE_LOG(3, "msg.index   = %d\n", msg->index);
            SPACE(1);
            VERBOSE_LOG(3, "msg.size    = %d\n", msg->size);
            SPACE(1);
            VERBOSE_LOG(3, "msg.data    = %d\n", bytestoi(msg->data));
            SPACE(1);
            VERBOSE_LOG(3, "msg.data    = %lu\n", stol(msg->data + 4));
            SPACE(1);
            VERBOSE_LOG(3, "msg.data    = %lu\n", stol(msg->data + 12));
            break;
        case BARR:
            VERBOSE_LOG(3, "msg.op      = BARR     \n");
            SPACE(1);
            VERBOSE_LOG(3, "msg.frompid = %d\n", msg->frompid);
            SPACE(1);
            VERBOSE_LOG(3, "msg.topid   = %d\n", msg->topid);
            SPACE(1);
            VERBOSE_LOG(3, "msg.temp     =%d\n", msg->temp);
            SPACE(1);
            VERBOSE_LOG(3, "msg.seqno   = %d\n", msg->seqno);
            SPACE(1);
            VERBOSE_LOG(3, "msg.index   = %d\n", msg->index);
            SPACE(1);
            VERBOSE_LOG(3, "msg.size    = %d\n", msg->size);
            SPACE(1);
            VERBOSE_LOG(3, "msg.data    = %d\n",
                        bytestoi(msg->data)); // lock (4bytes)
            SPACE(1);
            VERBOSE_LOG(3, "msg.data    = %lu\n", stol(msg->data + 4));
            SPACE(1);
            VERBOSE_LOG(3, "msg.data    = %lu\n", stol(msg->data + 12));
            break;
        case BARRGRANT:
            VERBOSE_LOG(3, "msg.op      = BARRGRANT\n");
            SPACE(1);
            VERBOSE_LOG(3, "msg.frompid = %d\n", msg->frompid);
            SPACE(1);
            VERBOSE_LOG(3, "msg.topid   = %d\n", msg->topid);
            SPACE(1);
            VERBOSE_LOG(3, "msg.temp     =%d\n", msg->temp);
            SPACE(1);
            VERBOSE_LOG(3, "msg.seqno   = %d\n", msg->seqno);
            SPACE(1);
            VERBOSE_LOG(3, "msg.index   = %d\n", msg->index);
            SPACE(1);
            VERBOSE_LOG(3, "msg.size    = %d\n", msg->size);
            SPACE(1);
            VERBOSE_LOG(3, "msg.data    = %d\n",
                        bytestoi(msg->data)); // lock (4bytes)
            SPACE(1);
            VERBOSE_LOG(3, "msg.data    = %lu\n", stol(msg->data + 4));
            SPACE(1);
            VERBOSE_LOG(3, "msg.data(from)    = %d\n",
                        bytestoi(msg->data + 12));
            break;
        case WAIT:
            VERBOSE_LOG(3, "msg.op      = WAIT     \n");
            SPACE(1);
            VERBOSE_LOG(3, "msg.frompid = %d\n", msg->frompid);
            SPACE(1);
            VERBOSE_LOG(3, "msg.topid   = %d\n", msg->topid);
            SPACE(1);
            VERBOSE_LOG(3, "msg.temp     =%d\n", msg->temp);
            SPACE(1);
            VERBOSE_LOG(3, "msg.seqno   = %d\n", msg->seqno);
            SPACE(1);
            VERBOSE_LOG(3, "msg.index   = %d\n", msg->index);
            SPACE(1);
            VERBOSE_LOG(3, "msg.size    = %d\n", msg->size);
            break;
        case WAITGRANT:
            VERBOSE_LOG(3, "msg.op      = WAITGRANT\n");
            SPACE(1);
            VERBOSE_LOG(3, "msg.frompid = %d\n", msg->frompid);
            SPACE(1);
            VERBOSE_LOG(3, "msg.topid   = %d\n", msg->topid);
            SPACE(1);
            VERBOSE_LOG(3, "msg.temp     =%d\n", msg->temp);
            SPACE(1);
            VERBOSE_LOG(3, "msg.seqno   = %d\n", msg->seqno);
            SPACE(1);
            VERBOSE_LOG(3, "msg.index   = %d\n", msg->index);
            SPACE(1);
            VERBOSE_LOG(3, "msg.size    = %d\n", msg->size);
            break;
        case REL:
            VERBOSE_LOG(3, "msg.op      = REL      \n");
            SPACE(1);
            VERBOSE_LOG(3, "msg.frompid = %d\n", msg->frompid);
            SPACE(1);
            VERBOSE_LOG(3, "msg.topid   = %d\n", msg->topid);
            SPACE(1);
            VERBOSE_LOG(3, "msg.temp     =%d\n", msg->temp);
            SPACE(1);
            VERBOSE_LOG(3, "msg.seqno   = %d\n", msg->seqno);
            SPACE(1);
            VERBOSE_LOG(3, "msg.index   = %d\n", msg->index);
            SPACE(1);
            VERBOSE_LOG(3, "msg.size    = %d\n", msg->size);
            SPACE(1);
            VERBOSE_LOG(3, "msg.data    = %d\n",
                        bytestoi(msg->data)); // lock (4bytes)
            SPACE(1);
            VERBOSE_LOG(3, "msg.data    = %lu\n", stol(msg->data + 4));
            SPACE(1);
            VERBOSE_LOG(3, "msg.data(from)    = %d\n",
                        bytestoi(msg->data + 12));
            break;
        case WTNT:
            VERBOSE_LOG(3, "msg.op      = WTNT     \n");
            SPACE(1);
            VERBOSE_LOG(3, "msg.frompid = %d\n", msg->frompid);
            SPACE(1);
            VERBOSE_LOG(3, "msg.topid   = %d\n", msg->topid);
            SPACE(1);
            VERBOSE_LOG(3, "msg.temp     =%d\n", msg->temp);
            SPACE(1);
            VERBOSE_LOG(3, "msg.seqno   = %d\n", msg->seqno);
            SPACE(1);
            VERBOSE_LOG(3, "msg.index   = %d\n", msg->index);
            SPACE(1);
            VERBOSE_LOG(3, "msg.size    = %d\n", msg->size);
            SPACE(1);
            VERBOSE_LOG(3, "msg.data    = %d\n",
                        bytestoi(msg->data)); // lock (4bytes)
            SPACE(1);
            VERBOSE_LOG(3, "msg.data    = %lu\n", stol(msg->data + 4));
            SPACE(1);
            VERBOSE_LOG(3, "msg.data    = %lu\n", stol(msg->data + 12));
            break;
        case STAT:
            VERBOSE_LOG(3, "msg.op      = STAT     \n");
            SPACE(1);
            VERBOSE_LOG(3, "msg.frompid = %d\n", msg->frompid);
            SPACE(1);
            VERBOSE_LOG(3, "msg.topid   = %d\n", msg->topid);
            SPACE(1);
            VERBOSE_LOG(3, "msg.temp     =%d\n", msg->temp);
            SPACE(1);
            VERBOSE_LOG(3, "msg.seqno   = %d\n", msg->seqno);
            SPACE(1);
            VERBOSE_LOG(3, "msg.index   = %d\n", msg->index);
            SPACE(1);
            VERBOSE_LOG(3, "msg.size    = %d\n", msg->size);
            break;
        case STATGRANT:
            VERBOSE_LOG(3, "msg.op      = STATGRANT\n");
            SPACE(1);
            VERBOSE_LOG(3, "msg.frompid = %d\n", msg->frompid);
            SPACE(1);
            VERBOSE_LOG(3, "msg.topid   = %d\n", msg->topid);
            SPACE(1);
            VERBOSE_LOG(3, "msg.temp     =%d\n", msg->temp);
            SPACE(1);
            VERBOSE_LOG(3, "msg.seqno   = %d\n", msg->seqno);
            SPACE(1);
            VERBOSE_LOG(3, "msg.index   = %d\n", msg->index);
            SPACE(1);
            VERBOSE_LOG(3, "msg.size    = %d\n", msg->size);
            break;
        case JIAEXIT:
            VERBOSE_LOG(3, "msg.op      = JIAEXIT  \n");
            SPACE(1);
            VERBOSE_LOG(3, "msg.frompid = %d\n", msg->frompid);
            SPACE(1);
            VERBOSE_LOG(3, "msg.topid   = %d\n", msg->topid);
            SPACE(1);
            VERBOSE_LOG(3, "msg.temp     =%d\n", msg->temp);
            SPACE(1);
            VERBOSE_LOG(3, "msg.seqno   = %d\n", msg->seqno);
            SPACE(1);
            VERBOSE_LOG(3, "msg.index   = %d\n", msg->index);
            SPACE(1);
            VERBOSE_LOG(3, "msg.size    = %d\n", msg->size);
            SPACE(1);
            VERBOSE_LOG(3, "msg.data    = %lu\n", stol(msg->data));
            SPACE(1);
            VERBOSE_LOG(3, "msg.data    = %lu\n", stol(msg->data + 8));
            SPACE(1);
            VERBOSE_LOG(3, "msg.data    = %lu\n", stol(msg->data + 16));
            break;
        default:
            VERBOSE_LOG(3, "msg.op      = %d       \n", msg->op);
            SPACE(1);
            VERBOSE_LOG(3, "msg.frompid = %d\n", msg->frompid);
            SPACE(1);
            VERBOSE_LOG(3, "msg.topid   = %d\n", msg->topid);
            SPACE(1);
            VERBOSE_LOG(3, "msg.temp     =%d\n", msg->temp);
            SPACE(1);
            VERBOSE_LOG(3, "msg.seqno   = %d\n", msg->seqno);
            SPACE(1);
            VERBOSE_LOG(3, "msg.index   = %d\n", msg->index);
            SPACE(1);
            VERBOSE_LOG(3, "msg.size    = %d\n", msg->size);
            SPACE(1);
            VERBOSE_LOG(3, "msg.data    = %lu\n", stol(msg->data));
            SPACE(1);
            VERBOSE_LOG(3, "msg.data    = %lu\n", stol(msg->data + 8));
            SPACE(1);
            VERBOSE_LOG(3, "msg.data    = %lu\n", stol(msg->data + 16));
            break;
        }
    }
}

extern void setwtvect(int homei, wtvect_t wv);

/**
 * @brief Turn optimizations such as (home migration, write vector, adaptive
 * write detection) on
 *
 * @param dest: optimization technique
 * @param value ON, turn on the dest; OFF, turn off the dest
 */
void jia_config(int dest, int value) {
    int i;
    switch (dest) {
    case HMIG:
        H_MIG =
            value; /* change optimization 'home migration' to value(ON/OFF) */
        break;
    case ADWD:
        AD_WD =
            value; /* change optimization 'adaptive write detetion' to value*/
        break;
    case BROADCAST:
        B_CAST = value;
        break;
    case LOADBAL:
        LOAD_BAL = value;
        if (LOAD_BAL == ON)
            firsttime = 1;
        caltime = 0.0;
        break;
    case WVEC:
        if ((W_VEC == OFF) &&
            (value == ON)) { /*  change optimization 'write vector' to value */
            for (i = 0; i < Homepages; i++) {
                home[i].wtvect =
                    (wtvect_t *)malloc(system_setting.hostc * sizeof(wtvect_t));
                setwtvect(i, WVFULL);
            }
        } else if ((W_VEC == ON) && (value == OFF)) {
            for (i = 0; i < Homepages; i++) {
                free(home[i].wtvect);
            }
        }
        W_VEC = value;
        break;
    case RDMA:
        VERBOSE_LOG(3, "TODO! RDMA support");
        break;
    default:
        VERBOSE_LOG(3, "Null configuration!\n");
        break;
    }
}

/**
 * @brief emptyprintf -- do nothing
 *
 */
void emptyprintf() {}

/************ time ****************/

/*-----------------------------------------------------------*/
/* Following programs are used by Shi. 9.10 */

unsigned long start_sec = 0;
unsigned long start_time_sec = 0;
unsigned long start_time_usec = 0;
unsigned long start_msec = 0;

/**
 * @brief jia_current_time -- Return the time, in milliseconds, elapsed after
 * the first call to this routine.
 *
 * @return unsigned long milliseconds value
 */
unsigned long jia_current_time() {
    struct timespec clock;

    /*
     * Return the time, in milliseconds, elapsed after the first call
     * to this routine.
     */
    clock_gettime(CLOCK_REALTIME, &clock);
    // if (!start_sec) {
    //     start_sec = clock.tv_sec;
    //     start_msec = (unsigned long)(clock.tv_nsec / 1000000);
    // }
    // return (1000 * (clock.tv_sec - start_sec) +
    //         (clock.tv_nsec / 1000000 - start_msec));
    return (1000000000 * clock.tv_sec + clock.tv_nsec);
}

/*-----------------------------------------------------------*/
/**
 * @brief jia_clock() - calculate the elapsed time since program started
 *
 * @return float: time(us) elapsed since program started
 */
float jia_clock() {
    float time;

    struct timeval val;
    gettimeofday(&val, NULL); // get current system time and store it into val
    // if (!start_time_sec) { // if not set initial start time, set current time
    // to
    //                        // be start time
    //     start_time_sec = val.tv_sec;
    //     start_time_usec = val.tv_usec;
    // }

    time = (float)((val.tv_sec * 1000000.0 + val.tv_usec * 1.0 -
                    start_time_sec * 1000000.0 - start_time_usec * 1.0) /
                   1000000.0);
    return (time);
}

/**
 * @brief get_usecs -- Get the usecs object
 *
 * @return unsigned int the elapsed time since base (microsecond)
 */
unsigned int get_usecs() {
    static struct timeval base;
    struct timeval time;

    gettimeofday(&time, NULL);
    if (!base.tv_usec) { // if base has been initialized
        base = time;
    }
    return ((time.tv_sec - base.tv_sec) * 1000000 +
            (time.tv_usec - base.tv_usec));
}

#else /* NULL_LIB */
#include <sys/time.h>

unsigned long start_sec = 0;
unsigned long start_time_sec = 0;
unsigned long start_time_usec = 0;
unsigned long start_msec = 0;
float jia_clock() {
    extern void setwtvect(int homei, wtvect_t wv);
}
return (float)(((val.tv_sec * 1000000.0 + val.tv_usec) -
                (start_time_sec * 1000000.0 + start_time_usec)) /
               1000000.0);
}

void jia_error(char *errstr) {
    VERBOSE_LOG(3, "JIAJIA error --- %s\n", errstr);
    exit(-1);
}
#endif /* NULL_LIB */