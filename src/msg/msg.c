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
#include "stat.h"
#include "msg.h"
#include "load.h"
#include <stdatomic.h>

extern char errstr[Linesize];

volatile int recvwait, endofmsg;
jia_msg_t msgbuf[Maxmsgbufs]; /* message buffer */
unsigned long msgseqno;
msg_buffer_t msg_buffer = {0};

/**
 * @brief initmsg -- initialize msgbuf and global vars
 *
 */
void initmsg() {

    init_msg_buffer(&msg_buffer, system_setting.msg_buffer_size);

    int i;

    endofmsg = 0;
    recvwait = 0;
    msgseqno = 0;
    for (i = 0; i < Maxmsgbufs; i++) {
        msgbuf[i].op = ERRMSG;
        msgbuf[i].frompid = 0;
        msgbuf[i].topid = 0;
        msgbuf[i].scope = 0;
        msgbuf[i].temp = 0;
        msgbuf[i].index = 0;
        msgbuf[i].size = 0;
        msgbuf[i].seqno = 0;
    }
}

int init_msg_buffer(msg_buffer_t *msg_buffer, int size) {
    if (size <= 0) {
        size = system_setting.msg_buffer_size;
    }

    msg_buffer->buffer = (slot_t *)malloc(sizeof(slot_t) * size);
    if (msg_buffer->buffer == NULL) {
        perror("msg_buffer malloc");
        return -1;
    }

    // initialize buffer size
    msg_buffer->size = size;

    // initialize semaphores
    if (sem_init(&(msg_buffer->count), 0, size) != 0) {
        perror("msg_buffer sem init");
        free(msg_buffer->buffer);
        return -1;
    }

    // initialize buffer slot
    for (int i = 0; i < size; i++) {
        msg_buffer->buffer[i].msg = (jia_msg_t){0};
        msg_buffer->buffer[i].msg.op = ERRMSG;
        // msg_buffer->buffer[i].state = SLOT_FREE;
        atomic_init(&msg_buffer->buffer[i].state, SLOT_FREE);
        // if(pthread_mutex_init(&(msg_buffer->buffer[i].lock), NULL)!= 0) {
        //     perror("msg_buffer mutex/cond init");
        //     for(int j = 0; j < i; j++) {
        //         pthread_mutex_destroy(&(msg_buffer->buffer[j].lock));
        //     }
        //     sem_destroy(&(msg_buffer->count));
        //     free(msg_buffer->buffer);
        //     return -1;
        // }
    }

    return 0;
}

void free_msg_buffer(msg_buffer_t *msg_buffer) {
    for (int i = 0; i < msg_buffer->size; i++) {
        slot_t *slot = &msg_buffer->buffer[i];
        // pthread_mutex_destroy(&slot->lock);
    }
    sem_destroy(&msg_buffer->count);
    free(msg_buffer->buffer);
}

// int copy_msg_to_buffer(msg_buffer_t *buffer, jia_msg_t *msg)
// {
//     if (sem_wait(&msg_buffer.count) != 0) {
//         return -1;
//     }

//     for (int i = 0; i < msg_buffer.size; i++) {
//         slot_t *slot = &msg_buffer.buffer[i];
//         if(!pthread_mutex_trylock(&slot->lock)){
//             slot->state = SLOT_BUSY;
//             memcpy((void *)&slot->msg, (void *)msg, sizeof(jia_msg_t));
//             slot->state = SLOT_FREE;
//             return i;
//         }
//     }

//     sem_post(&msg_buffer.count);
//     return 0;
// }

int freemsg_lock(msg_buffer_t *buffer) {
    int semvalue;
    sem_getvalue(&msg_buffer.count, &semvalue);
    log_info(4, "pre freemsg_lock count value: %d", semvalue);
    if (sem_wait(&msg_buffer.count) != 0) {
        return -1;
    }

    for (int i = 0; i < msg_buffer.size; i++) {
        slot_state_t slot_state = SLOT_FREE;
        slot_t *slot = &msg_buffer.buffer[i];
        if (atomic_compare_exchange_weak(&slot->state, &slot_state, SLOT_BUSY))
            return i;
        // if(!pthread_mutex_trylock(&slot->lock)){
        //     slot->state = SLOT_BUSY;
        //     return i;
        // }
    }
    return -1;
}

void freemsg_unlock(msg_buffer_t *buffer, int index) {
    slot_t *slot = &msg_buffer.buffer[index];
    // slot->state = SLOT_FREE;
    atomic_store(&slot->state, SLOT_FREE);
    // pthread_mutex_unlock(&slot->lock);

    sem_post(&msg_buffer.count);

    int semvalue;
    sem_getvalue(&msg_buffer.count, &semvalue);
    log_info(4, "after freemsg_unlock count value: %d", semvalue);
}

int move_msg_to_outqueue(msg_buffer_t *buffer,
                         int index,
                         msg_queue_t *outqueue) {
    slot_t *slot = &msg_buffer.buffer[index];
    if (slot->msg.topid == system_setting.jia_pid) {
        msg_handle(&slot->msg);
    } else {
        int ret = enqueue(outqueue, &slot->msg);
        if (ret == -1) {
            perror("enqueue");
            return ret;
        }
    }
    return 0;
}

/******************** Message Passing Part*****************/

/**
 * @brief nextpacket - find next packet in the buf that meet fromproc and tag
 * condition
 *
 * @param fromproc
 * @param tag
 * @return int
 */
int nextpacket(int fromproc, int tag) {
    int i, index;
    unsigned long next;

    index = Maxmsgbufs;
    next = Maxmsgno;
    for (i = 0; i < Maxmsgbufs; i++) {
        /** must be MSG, its to/from proc and its tag/scope must be right */
        if (((msgbuf[i].op == MSGTAIL) || (msgbuf[i].op == MSGBODY)) &&
            ((fromproc == MSG_PROC_ALL) || (fromproc == msgbuf[i].frompid)) &&
            ((tag == MSG_TAG_ALL) || (tag == msgbuf[i].scope)) &&
            // get msg with strong sequence
            (msgbuf[i].seqno < next)) {

            next = msgbuf[i].seqno;
            index = i;
        }
    }
    return (index);
}

/**
 * @brief nextmsg - get a msg from msgbuf and put it into buf, return msg's size
 *
 * @param buf
 * @param len
 * @param fromproc
 * @param tag
 * @return int
 */
int nextmsg(char *buf, int len, int fromproc, int tag) {
    int i = 0;
    int msgsize, size;

    // TODO: recvwait should be atomic 
    msgsize = 0;
    while ((endofmsg == 0) && (i != Maxmsgbufs)) {

        /** step 1: use recvwait to sync */
        recvwait = 1;
        while (recvwait)
            ;

        /** step 2: get next msg and clear msgbuf */
        i = nextpacket(fromproc, tag);
        msgbuf[i].op = ERRMSG;
        if (msgbuf[i].op == MSGTAIL)
            endofmsg = 1;

        /** step 3: memcpy msg */
        size = MIN(msgbuf[i].size, len - msgsize);
        if (size < 0)
            size = 0;
        memcpy(buf + msgsize, msgbuf[i].data, size);
        msgsize += size;
    }

    return (msgsize);
}

/**
 * @brief bsendmsg -- broadcast msg
 *
 * @param msg
 */
void bsendmsg(jia_msg_t *msg) {
    unsigned int root, level;
    
    msg->op += BCAST; // op >= BCAST, always call bcastserver
    root = system_setting.jia_pid; // current host as root

    if (system_setting.hostc == 1) {
        level = 1;
    } else {
        // set level
        for (level = 0; (1 << level) < system_setting.hostc; level++)
            ;
    }

    // 8 bytes for (root(4 bytes)|level(4 bytes))
    msg->temp = ((root & 0xffff) << 16) | (level & 0xffff);
    bcastserver(msg);
}

/**
 * @brief msg_handle - handle msg
 * 
 * @param msg 
 * @note msg_handle called by server_thread
 */
void msg_handle(jia_msg_t *msg) {
    log_info(3, "In servermsg!");

    switch (msg->op) {
    case DIFF:
        diffserver(msg);
        break;
    case DIFFGRANT:
        diffgrantserver(msg);
        break;
    case GETP:
        getpserver(msg);
        break;
    case GETPGRANT:
        getpgrantserver(msg);
        break;
    case ACQ:
        acqserver(msg);
        break;
    case ACQGRANT:
        acqgrantserver(msg);
        break;
    case INVLD:
        invserver(msg);
        break;
    case BARR:
        barrserver(msg);
        break;
    case BARRGRANT:
        barrgrantserver(msg);
        break;
    case REL:
        relserver(msg);
        break;
    case WTNT:
        wtntserver(msg);
        break;
    case JIAEXIT:
        jiaexitserver(msg);
        break;
    case WAIT:
        waitserver(msg);
        break;
    case WAITGRANT:
        waitgrantserver(msg);
        break;
    case STAT:
        statserver(msg);
        break;
    case STATGRANT:
        statgrantserver(msg);
        break;
    case SETCV:
        setcvserver(msg);
        break;
    case RESETCV:
        resetcvserver(msg);
        break;
    case WAITCV:
        waitcvserver(msg);
        break;
    case CVGRANT:
        cvgrantserver(msg);
        break;
    case MSGBODY:
    case MSGTAIL:
        msgrecvserver(msg);
        break;
    case LOADREQ:
        loadserver(msg);
        break;
    case LOADGRANT:
        loadgrantserver(msg);
        break;

    default:
        if (msg->op >= BCAST) {
            bcastserver(msg);
        } else {
            printmsg(msg);
            local_assert(0, "msgserver(): Incorrect Message!");
        }
        break;
    }
    log_info(3, "Out servermsg!\n");
}

#else /* NULL_LIB */

#endif /* NULL_LIB */
