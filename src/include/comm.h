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
 **********************************************************************/

#ifndef JIACOMM_H
#define JIACOMM_H
#pragma once

#include "global.h"
#include "init.h"
#include "msg.h"
#include "semaphore.h"

#define TIMEOUT 300   /* used to wait for ack */
#define MAX_RETRIES 64 /* number of retransmissions */

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
jia_msg_t *dequeue(msg_queue_t *msg_queue);

/**
 * @brief free_queue - free queue
 * 
 * @param queue msg queue
 */
void free_msg_queue(msg_queue_t *queue);

/* function declaration  */

/**
 * @register_sigint_handler -- register signal handler
 */
void register_sigint_handler();

extern msg_queue_t inqueue;
extern msg_queue_t outqueue;
extern long start_port;

#endif /* JIACOMM_H */
