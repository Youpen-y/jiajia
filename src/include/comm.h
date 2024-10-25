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

/* msg operation (type) */
#define DIFF        0
#define DIFFGRANT   1
#define GETP        2
#define GETPGRANT   3
#define ACQ         4
#define ACQGRANT    5
#define INVLD       6
#define BARR        7
#define BARRGRANT   8
#define REL         9
#define WTNT        10
#define JIAEXIT     11
#define WAIT        12
#define WAITGRANT   13
#define STAT        14
#define STATGRANT   15
#define ERRMSG      16

#define SETCV       17
#define RESETCV     18
#define WAITCV      19
#define CVGRANT     20
#define MSGBODY     21
#define MSGTAIL     22
#define LOADREQ     23
#define LOADGRANT   24
#define BCAST       100

typedef struct jia_msg {
    unsigned int op;      /* operation type */
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

typedef struct msg_queue_slot {
    jia_msg_t msg;
    pthread_mutext_t lock;
} msg_queue_slot_t;

typedef struct msg_queue {
    msg_queue_slot_t *queue;
    int               head;
    int               tail;
    int               size;
    sem_t             busy_count;
    sem_t             free_count;
} msg_queue_t;

extern msg_queue_t inqueue;
extern msg_queue_t outqueue;

/**
 * @brief 
 * 
 * @param queue 
 * @param size
 * @return int 
 */
int init_queue(msg_queue_t *queue, int size);


/**
 * @brief 
 * 
 * @param queue 
 * @param msg 
 * @return int 
 */
int enqueue(msg_queue_t *queue, jia_msg_t *msg);

/**
 * @brief 
 * 
 * @param queue 
 * @return jia_msg_t* 
 */
jia_msg_t *dequeue(msg_queue_t *queue);

/**
 * @brief free_queue - free queue
 * 
 * @param queue
 */
void free_queue(msg_queue_t *queue);


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


typedef struct {
    jia_msg_t *msgarray;
    int       *msgbusy;
    int        size;
} msg_buffer_t;

// extern variables
extern msg_buffer_t msg_buffer;


// #define inqh inqueue[inhead]    // inqueue msg head
// #define inqt inqueue[intail]    // inqueue msg tail
// #define outqh outqueue[outhead] // outqueue msg head
// #define outqt outqueue[outtail] // outqueue msg tail
#define STATOP(op) if(statflag){op};

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
 * @brief init_msg_buffer -- initialize msg array and corresponding flag that indicate busy or free
 * 
 */
void init_msg_buffer();


/**
 * @brief free_msg_buffer -- free msg array and corresponding flag that indicate busy or free
 * 
 */
void free_msg_buffer();


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
