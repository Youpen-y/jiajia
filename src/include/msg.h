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

#include "semaphore.h"
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

#define Maxsize 40960
#define Msgheadsize 32                   /* fixed header of msg */
#define Maxmsgsize (Maxsize - Msgheadsize) /* data size of msg */
#define Maxqueue                                                               \
    32 /* size of input and output queue for communication (>= 2*maxhosts)*/

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

typedef struct slot {
    jia_msg_t msg;
    _Atomic volatile slot_state_t state;
} slot_t;
typedef struct {
    slot_t *buffer;
    int        size;
    sem_t      count;   // available slot count
} msg_buffer_t;

typedef struct msg_queue {
    unsigned char   **queue;    // msg queue
    int               size;     // size of queue(must be power of 2)

    pthread_mutex_t   head_lock;    // lock for head
    pthread_mutex_t   tail_lock;    // lock for tail
    pthread_mutex_t   post_lock;    // lock for post(rdma)
    volatile unsigned               head;         // head
    volatile unsigned               tail;         // tail
    volatile unsigned               post;         // (rdma)

    sem_t             busy_count;   // busy slot count
    sem_t             free_count;   // free slot count
    
    _Atomic volatile unsigned  busy_value;
    _Atomic volatile unsigned  free_value;
    _Atomic volatile unsigned  post_value;  // (rdma)
} msg_queue_t;

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
 * @brief copy_msg_to_buffer - commit msg to msg buffer
 * 
 * @param buffer msg buffer
 * @param msg msg that will be sent (sent to buffer first)
 * @return int 0 if success, -1 if failed
 */
int copy_msg_to_buffer(msg_buffer_t *buffer, jia_msg_t *msg);


/**
 * @brief freemsg_lock - get a free msg index from msg buffer and lock it
 * 
 * @param buffer msg buffer
 * @return int i represent the index of msg that is locked if success, -1 if failed
 */
int freemsg_lock(msg_buffer_t *buffer);


/**
 * @brief freemsg_unlock - unlock msg index and return it to msg buffer
 *
 * @param buffer msg buffer
 * @param index index of occupied msg slot of msg buffer
 */
void freemsg_unlock(msg_buffer_t *buffer, int index);


/**
 * @brief move_msg_to_outqueue - move msg (i.e. msg_buffer->buffer[index]) from msg buffer to outqueue
 * 
 * @param buffer msg buffer
 * @param index index of msg slot of msg buffer that will be moved to outqueue
 * @param outqueue outqueue
 * @return int 
 */
int move_msg_to_outqueue(msg_buffer_t *buffer, int index, msg_queue_t *outqueue);


/**
 * @brief msg_handle - handle msg
 * 
 * @param msg 
 * @note msg_handle called by server_thread
 */
void msg_handle(jia_msg_t *msg);


/**
 * @brief jia_recv --
 *
 * @param buf
 * @param len
 * @param fromproc
 * @param tag
 * @return int
 */
int jia_recv(char *buf, int len, int fromproc, int tag);


/**
 * @brief jia_send -- send len bytes message from buf to host toproc with tag
 *
 * @param buf message source
 * @param len length of message (total, per bytes)
 * @param toproc destination
 * @param tag set msg'scope to tag
 */
void jia_send(char *buf, int len, int toproc, int tag);


/**
 * @brief jia_reduce -- reduce msg to one host
 *
 * @param sendbuf message source
 * @param recvbuf message dest
 * @param count data count
 * @param op msg's reduce op
 * @param root src host's pid
 */
void jia_reduce(char *sendbuf, char *recvbuf, int count, int op, int root);


/**
 * @brief jia_bcast -- broadcast one msg to every host
 *
 * @param buf message source
 * @param len msg's length
 * @param root src host's pid
 */
void jia_bcast(char *buf, int len, int root);


/**
 * @brief msgrecvserver -- put the msg req into the first empty space in msgbuf
 *
 * @param req msg
 */
void msgrecvserver(jia_msg_t *req);


/**
 * @brief bcastserver --
 *
 * @param msg
 */
void bcastserver(jia_msg_t *msg);

#endif /*JIAMSG_H*/
