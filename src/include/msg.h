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

#ifndef JIAMSG_H
#define JIAMSG_H

#include "global.h"
#define Maxmsgbufs     48
#define Maxmsgno       0x40000000
#define MSG_TAG_ALL    (-1)
#define MSG_PROC_ALL   (-1)
#define REDUCE_TAG     62559641
#define BCAST_TAG      62564392

#define SUM_INT           0
#define MAX_INT           1
#define MIN_INT           2
#define SUM_FLOAT        10
#define MAX_FLOAT        11
#define MIN_FLOAT        12
#define SUM_DOUBLE       20
#define MAX_DOUBLE       21
#define MIN_DOUBLE       22


typedef enum {
    DIFF,
    DIFFGRANT,
    GETP,
    GETPGRANT,
    ACQ,
    ACQGRANT,
    INVLD,
    BARR,
    BARRGRANT,
    REL,
    WTNT,
    JIAEXIT,
    WAIT,
    WAITGRANT,
    STAT,
    STATGRANT,
    ERRMSG,
    SETCV,
    RESETCV,
    WAITCV,
    CVGRANT,
    MSGBODY,
    MSGTAIL,
    LOADREQ,
    LOADGRANT,
    BCAST = 100,
} msg_op_t;

typedef struct jia_msg {
    msg_op_t     op;     /* msg operation type */
    unsigned int frompid; /* from pid */
    unsigned int topid;   /* to pid */
    unsigned int temp;    /* Useless (flag to indicate read or write request)*/
    unsigned int seqno;   /* sequence number */
    unsigned int index;   /* msg index in msg array */
    unsigned int scope;   /* Inca. no.  used as tag in msg. passing */
    unsigned int size;    /* data size */
    /* header is 32 bytes */
    unsigned char data[Maxmsgsize];
} jia_msg_t;



typedef enum {
    SLOT_FREE = 0,  // slot is free
    SLOT_BUSY = 1,  // slot is busy
} slot_state_t;

typedef struct buffer_slot {
    jia_msg_t msg;
    volatile slot_state_t state;
    pthread_mutex_t lock;
    pthread_cond_t  cond;
} buffer_slot_t;
typedef struct {
    buffer_slot_t *buffer;
    int        size;

    sem_t      busy_count;
    sem_t      free_count;
} msg_buffer_t;

// extern variables
extern msg_buffer_t msg_buffer;


/**
 * @brief init_msg_buffer - initialize msg buffer with specified size
 *
 * @param msg_buffer msg buffer
 * @param size if size <= 0, use default size (i.e. system_setting.msg_buffer_size)
 * @return int 0 if success, -1 if failed
 */
int init_msg_buffer(msg_buffer_t *msg_buffer, int size);


/**
 * @brief free_msg_buffer - free msg buffer
 * 
 * @param msg_buffer 
 */
void free_msg_buffer(msg_buffer_t *msg_buffer);

/**
 * @brief commit_msg_to_buffer - commit msg to msg buffer
 * 
 * @param buffer msg buffer
 * @param msg msg that will be sent (sent to buffer first)
 * @return int 0 if success, -1 if failed
 */
int commit_msg_to_buffer(msg_buffer_t *buffer, jia_msg_t *msg);


/**
 * @brief free_msg_index_lock - get a free msg index from msg buffer and lock it
 * 
 * @param buffer msg buffer
 * @return int i represent the index of msg that is locked if success, -1 if failed
 */
int free_msg_index_lock(msg_buffer_t *buffer);


/**
 * @brief free_msg_index_unlock - unlock msg index and return it to msg buffer
 *
 * @param buffer msg buffer
 * @param index index of occupied msg slot of msg buffer
 */
void free_msg_index_unlock(msg_buffer_t *buffer, int index);


/**
 * @brief move_msg_to_outqueue - move msg (i.e. msg_buffer->buffer[index]) from msg buffer to outqueue
 * 
 * @param buffer msg buffer
 * @param index index of msg slot of msg buffer that will be moved to outqueue
 * @param outqueue outqueue
 * @return int 
 */
int move_msg_to_outqueue(msg_buffer_t *buffer, int index, msg_queue_t *outqueue);



void msgrecvserver(jia_msg_t *req);


#endif /*JIAMSG_H*/
