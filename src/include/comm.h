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

#ifndef JIACOMM_H
#define JIACOMM_H

#include "global.h"
#include "init.h"

#define TIMEOUT 1000   /* used to wait for ack */
#define MAX_RETRIES 64 /* number of retransmissions */

#define Msgheadsize 32                   /* fixed header of msg */
#define Maxmsgsize (40960 - Msgheadsize) /* data size of msg */
#define Maxqueue                                                               \
    32 /* size of input and output queue for communication (>= 2*maxhosts)*/

typedef enum {
    SLOT_FREE = 0,  // slot is free
    SLOT_BUSY = 1,  // slot is busy
} slot_state_t;

typedef struct msg_queue_slot {
    jia_msg_t msg;               // msg
    volatile slot_state_t state; // state of slot
    pthread_mutext_t lock;       // mutex lock for each slot
    pthread_cond_t cond;         // condition variable
} msg_queue_slot_t;

typedef struct msg_queue {
    msg_queue_slot_t *queue;    // msg queue
    int               size;     // size of queue

    pthread_mutex_t   head_lock;    // lock for head
    pthread_mutex_t   tail_lock;    // lock for tail
    int               head;         // head
    int               tail;         // tail

    sem_t             busy_count;   // busy slot count
    sem_t             free_count;   // free slot count
} msg_queue_t;

extern msg_queue_t inqueue;
extern msg_queue_t outqueue;

/**
 * @brief init_msg_queue - initialize msg queue with specified size
 * 
 * @param queue msg queue
 * @param size if size < 0, use default size (i.e. system_setting.msg_queue_size)
 * @return int 0 if success, -1 if failed
 */
int init_msg_queue(msg_queue_t *queue, int size);


/**
 * @brief enqueue - enqueue msg
 * 
 * @param queue msg queue
 * @param msg msg
 * @return int 0 if success, -1 if failed 
 */
int enqueue(msg_queue_t *queue, jia_msg_t *msg);

/**
 * @brief dequeue - dequeue msg
 * 
 * @param queue msg queue
 * @param msg msg
 * @return 0 if success, -1 if failed
 */
int dequeue(msg_queue_t *queue, jia_msg_t *msg);

/**
 * @brief free_queue - free queue
 * 
 * @param queue msg queue
 */
void free_msg_queue(msg_queue_t *queue);


// typedef struct CommManager {
//     int         snd_fds[Maxhosts];      // send file descriptor
//     fd_set      snd_set;             // send fd_set, use with `select`
//     int         snd_maxfd;              // max_fd, use with `select`
//     unsigned    snd_seq[Maxhosts]; // sequence number that used to acknowledge

//     int         rcv_fds[Maxhosts];      // read file descriptor
//     fd_set      rcv_set;             // read fd_set
//     int         rcv_maxfd;              // max_fd, use with `select`
//     unsigned    rcv_seq[Maxhosts]; // sequence number
// } CommManager;

typedef struct comm_manager {
    int         snd_fds[Maxhosts];  // send file descriptor
    unsigned    snd_seq[Maxhosts];  // sequence number that used to acknowledge
    unsigned short snd_ports[Maxhosts];

    int         rcv_fds[Maxhosts];  // read file descriptor
    unsigned    rcv_seq[Maxhosts];  // sequence number
    unsigned short rcv_ports[Maxhosts];
} comm_manager_t;

extern comm_manager_t req_manager;
extern comm_manager_t rep_manager;




/* function declaration  */

/**
 * @brief initcomm -- initialize communication setting
 *
 * step1: initialize msg array and correpsonding flag to indicate busy or free
 *
 * step2: initialize pointer that indicate head, tail and count of inqueue and
 * outqueue
 *
 * step3: register signal handler (SIGIO, SIGINT)
 *
 * step4: initialize comm ports (reqports, repports)
 *
 * step5: initialize comm manager (commreq, commrep)
 */
void initcomm();

/**
 * @brief req_fdcreate -- creat socket file descriptor used to send and recv
 * request
 *
 * @param i the index of host
 * @param flag 1 means sin_port = 0, random port; others means specified
 * sin_port = reqports[jia_pid][i]
 * @return int socket file descriptor
 * creat socket file descriptor(fd) used to send and recv request and bind it to
 * an address (ip/port combination)
 */
int req_fdcreate(int i, int flag);

/**
 * @brief rep_fdcreate -- create socket file descriptor(fd) used to send and
 * recv reply
 *
 * @param i the index of host [0, hostc)
 * @param flag equals to 1 means fd with random port, 0 means fd with specified
 * port(repports[jia_pid][i])
 * @return int socket file descriptor(fd)
 */
int rep_fdcreate(int i, int flag);


/**
 * @brief sigio_handler -- IO signal handler
 * 
 */
#if defined SOLARIS || defined IRIX62
void sigio_handler(int sig, siginfo_t *sip, ucontext_t *uap);
#endif /* SOLARIS */
#ifdef LINUX
void sigio_handler();
#endif
#ifdef AIX41
void sigio_handler();
#endif /* AIX41 */

/**
 * @brief sigint_handler -- interrupt signal handler
 * 
 */
void sigint_handler();

/**
 * @brief asendmsg() -- send msg to outqueue[outtail], and call outsend()
 *
 * @param msg msg that will be sent
 */
void asendmsg(jia_msg_t *msg);

/**
 * @brief msgserver -- according to inqueue head msg.op choose different server
 *
 */
void msgserver();

/**
 * @brief outsend -- outsend the outqueue[outhead] msg
 *
 */
void outsend();

/**
 * @brief bsendmsg -- broadcast msg
 *
 * @param msg 
 */
void bsendmsg(jia_msg_t *msg);

void bcastserver(jia_msg_t *msg);


int init_comm_manager();


#endif /* JIACOMM_H */
