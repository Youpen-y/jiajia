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
#pragma once

#include "global.h"
#include "init.h"
#include "msg.h"
#include "semaphore.h"

#define TIMEOUT 100   /* used to wait for ack */
#define MAX_RETRIES 64 /* number of retransmissions */

extern msg_queue_t inqueue;
extern msg_queue_t outqueue;

/* fd type */
enum FDCR_MODE{
    FDCR_SEND,  // send fd
    FDCR_RECV,  // recv fd
    FDCR_ACK    // ack fd
};

/* ack type */
typedef struct {
    int          seqno;     // sequence number
    int          sid;       // the ack is returned by the host sid
} ack_t;


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

/* communication manager */
typedef struct comm_manager {
    int         snd_fds;  // send file descriptor
    unsigned    snd_seq[Maxhosts];  // sequence number that used to acknowledge
    unsigned short snd_server_port; // snd server port is destination host's port

    int ack_fds;
    unsigned ack_seq[Maxhosts]; // TODO: consider using ack seq array
    unsigned short ack_port;

    int         rcv_fds[Maxhosts];  // read file descriptor
    unsigned    rcv_seq[Maxhosts];  // sequence number
    unsigned short rcv_ports[Maxhosts];
} comm_manager_t;

extern comm_manager_t comm_manager;



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
 * @brief sigio_handler -- IO signal handler
 * 
 */
void sigio_handler();

/**
 * @brief sigint_handler -- interrupt signal handler
 * 
 */
void sigint_handler();

/** 
 * @brief register_sigint_handler -- register sigint signal handler
 *
 */
static void register_sigint_handler();

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
 * @brief bsendmsg -- broadcast msg
 *
 * @param msg 
 */
void bsendmsg(jia_msg_t *msg);

void bcastserver(jia_msg_t *msg);

/**
 * @brief fd_create -- creat socket file descriptor used to send and recv
 * request
 *
 * @param i the index of host
 * @param flag 1 means sin_port = 0, random port; others means specified
 * sin_port = reqports[jia_pid][i]
 * @return int socket file descriptor
 * creat socket file descriptor(fd) used to send and recv request and bind it to
 * an address (ip/port combination)
 */
static int fd_create(int i, enum FDCR_MODE flag);

/**
 * @brief set_nonblocking - set socket to nonblocking mode
 *
 * @param sockfd
 */
static void set_nonblocking(int sockfd);


#endif /* JIACOMM_H */
