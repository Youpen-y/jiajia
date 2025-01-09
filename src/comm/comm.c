/**********************************************************************
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
 * =================================================================== *
 *   This software is ported to SP2 by                                 *
 *                                                                     *
 *         M. Rasit Eskicioglu                                         *
 *         University of Alberta                                       *
 *         Dept. of Computing Science                                  *
 *         Edmonton, Alberta T6G 2H1 CANADA                            *
 * =================================================================== *
 **********************************************************************/

#include "global.h"
#include <stdlib.h>
#ifndef NULL_LIB
#include <pthread.h>
#include <semaphore.h>
#include <sys/socket.h>
#include "comm.h" // statgrantserver,
#include "mem.h"  // diffserver, getserver, diffgrantserver, getpgrantserver
#include "setting.h"
#include "syn.h" // acqserver, inverver, relserver, wtntserver, barrserver, barrgrantserver, acqgrantserver
#include "tools.h" // jiaexitserver, jia_current_time, newmsg, printmsg, get_usecs, emptyprintf
// waitgrantserver, waitserver, setcvserver, resetcvserver, waitcvserver,
// cvgrantserver,
#include "load.h" // loadserver, loadgrantserver,
#include "msg.h"  // msgrecvserver
#include "stat.h" // statserver
#include "thread.h"
#include <stdatomic.h>

struct inqueue_arg args[Maxhosts];

// global variables

/* communication manager */
comm_manager_t comm_manager;

/* in/out queue */
msg_queue_t inqueue[Maxhosts];
msg_queue_t outqueue;

long start_port;

static int init_comm_manager();
static int fd_create(int i, enum FDCR_MODE flag);
static void sigint_handler();
static void sigio_handler();
static void register_sigint_handler();

pthread_t server_tid[Maxhosts];

// function definitions

/**
 * @brief initcomm -- initialize communication setting
 */
void initcomm() {
    int i, j, fd;


    if (system_setting.jia_pid == 0) {
        VERBOSE_LOG(3, "************Initialize Communication!*******\n");
    }
    VERBOSE_LOG(3, "current jia_pid = %d\n", system_setting.jia_pid);
    VERBOSE_LOG(3, " start_port = %u \n", start_port);

    /* step 1: init msg buffer */
    init_msg_buffer(&msg_buffer, system_setting.msg_buffer_size);

    /* step 2: init inqueue, outqueue msg queue */
    for (int i = 0; i < system_setting.hostc; i++) {
        init_msg_queue(&inqueue[i], system_setting.msg_queue_size);
    }
    init_msg_queue(&outqueue, system_setting.msg_queue_size);

    /* step 3: init comm manager */
    init_comm_manager();

    /* step 4: create client, server, listen threads */
    pthread_create(&client_tid, NULL, client_thread,
                   &outqueue); // create a new thread to send msg from outqueue
    for (int i = 0; i < system_setting.hostc; i++) {
        args[i].addr = &inqueue[i];
        args[i].id = i;
        pthread_create(&server_tid[i], NULL, server_thread,
                    &args[i]); // create a new thread to serve msg from inqueue
    }
    pthread_create(&listen_tid, NULL, listen_thread, NULL);

    /* step 5: register sigint handler */
    register_sigint_handler();
}

/**
 * @brief fd_create -- creat socket file descriptor used to send and recv
 * request
 *
 * @param i the index of host
 * @param flag fd create flag, FDCR_ACK for ack port / FDCR_RECV for recv port / FDCR_SEND for send port
 *
 * @return int socket file descriptor
 * creat socket file descriptor(fd) used to send and recv request and bind it to
 * an address (ip/port combination)
 */
static int fd_create(int i, enum FDCR_MODE flag) {
    int fd, res, size;
    struct sockaddr_in addr;

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    local_assert((fd != -1), "req_fdcreate()-->socket()");

    // there, change the socket send and recv buffer size mannually
    size = Maxmsgsize + Msgheadsize + 128;
    res = setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (char *)&size, sizeof(size));
    local_assert((res == 0), "req_fdcreate()-->setsockopt():SO_RCVBUF");

    size = Maxmsgsize + Msgheadsize + 128;
    res = setsockopt(fd, SOL_SOCKET, SO_SNDBUF, (char *)&size, sizeof(size));
    local_assert((res == 0), "req_fdcreate()-->setsockopt():SO_SNDBUF");

    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    switch (flag) {
        case FDCR_ACK:
            addr.sin_port = htons(comm_manager.ack_port);   // specified ack port
            break;
        case FDCR_RECV:
            addr.sin_port = htons(comm_manager.rcv_ports[i]);   // specified recv port
            break;
        case FDCR_SEND:
            addr.sin_port = htons(0);   // random send port 
            break;
    }

    res = bind(fd, (struct sockaddr *)&addr, sizeof(addr));
    local_assert((res == 0), "req_fdcreate()-->bind()");

    return fd;
}

/**
 * @brief register_sigint_handler -- register sigint signal handler
 *
 */
static void register_sigint_handler() {
    struct sigaction act;

    act.sa_handler = (void_func_handler)sigint_handler;
    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_NODEFER;
    if (sigaction(SIGINT, &act, NULL)) {
        local_assert(0, "segv sigaction problem");
    }
}

/**
 * @brief sigint_handler -- sigint handler
 *
 */
static void sigint_handler() {
    jia_assert(0, "Exit by user!!\n");
}

/**
 * @brief sigio_handler -- sigio handler (it can be interrupt)
 *
 */
static void sigio_handler() {
#ifdef DOSTAT
    register unsigned int begin;
    STATOP(jiastat.sigiocnt++; if (interruptflag == 0) {
        begin = get_usecs();
        if (jiastat.kernelflag == 0) {
            jiastat.usersigiocnt++;
        } else if (jiastat.kernelflag == 1) {
            jiastat.synsigiocnt++;
        } else if (jiastat.kernelflag == 2) {
            jiastat.segvsigiocnt++;
        }
    } interruptflag++;)
#endif

#ifdef DOSTAT
    STATOP(interruptflag--; if (interruptflag == 0) {
        if (jiastat.kernelflag == 0) {
            jiastat.usersigiotime += get_usecs() - begin;
        } else if (jiastat.kernelflag == 1) {
            jiastat.synsigiotime += get_usecs() - begin;
        } else if (jiastat.kernelflag == 2) {
            jiastat.segvsigiotime += get_usecs() - begin;
        }
    })
#endif
}

int init_msg_queue(msg_queue_t *msg_queue, int size) {

    /** step 1: allocate memory size for msg_queue */ 
    if (size <= 0) {
        size = system_setting.msg_queue_size;
    }
    msg_queue->queue = (slot_t *)malloc(sizeof(slot_t) * size);
    if (msg_queue->queue == NULL) {
        perror("msg_queue malloc");
        return -1;
    }

    /** step 2: init msg_queue */ 
    msg_queue->size = size;
    msg_queue->head = 0;
    msg_queue->tail = 0;

    /** step 3: initialize head mutex and tail mutex */ 
    if (pthread_mutex_init(&(msg_queue->head_lock), NULL) != 0 ||
        pthread_mutex_init(&(msg_queue->tail_lock), NULL) != 0) {
        perror("msg_queue mutex init");
        free(msg_queue->queue);
        return -1;
    }

    /** step 4: initialize semaphores (or atomic count)*/
    if (sem_init(&(msg_queue->busy_count), 0, 0) != 0 ||
        sem_init(&(msg_queue->free_count), 0, size) != 0) {
        perror("msg_queue sem init");
        pthread_mutex_destroy(&(msg_queue->head_lock));
        pthread_mutex_destroy(&(msg_queue->tail_lock));
        free(msg_queue->queue);
        return -1;
    }

    // atomic_init(&msg_queue->busy_count, 0);
    // atomic_init(&msg_queue->free_count, size);

    /** step 5:init slot's state */
    for (int i = 0; i < size; i++) {
        msg_queue->queue[i].state = SLOT_FREE;
    }

    return 0;
}

int enqueue(msg_queue_t *msg_queue, jia_msg_t *msg) {
    unsigned current_value;
    unsigned slot_index;
    if (msg_queue == NULL || msg == NULL) {
        log_err("msg_queue or msg is NULL[msg_queue: %lx msg: %lx]",
                (long unsigned)msg_queue, (long unsigned)msg);
        return -1;
    }
    char *queue = (msg_queue == &outqueue) ? "outqueue" : "inqueue";

    // log_info(4, "pre %s enqueue free_count value: %d", queue,
    //          msg_queue->free_count);
    int semvalue;
    sem_getvalue(&msg_queue->free_count, &semvalue);
    log_info(4, "pre %s enqueue free_count value: %d", queue, semvalue);
    // wait for free slot
    if (sem_wait(&msg_queue->free_count) != 0) {
        log_err("sem_wait error");
        return -1;
    }
    sem_getvalue(&msg_queue->free_count, &semvalue);
    log_info(4, "enter %s enqueue! free_count value: %d", queue, semvalue);
    // /**
    //  * step 1: get current value
    //  * step 2: we will atomic sub busy_count iff current value > 0
    //  */
    // while (1) {
    //     current_value = atomic_load(&msg_queue->free_count);
    //     if (current_value > 0) {
    //         if (atomic_compare_exchange_weak(&msg_queue->free_count,
    //                                          &current_value,
    //                                          current_value - 1)) {
    //             break;
    //         }
    //     }
    // }
    // log_info(4, "enter %s enqueue! free_count value: %d", queue,
    //          current_value - 1);

    // lock tail and update tail pointer
    pthread_mutex_lock(&(msg_queue->tail_lock));
    slot_index = msg_queue->tail;
    msg_queue->tail = (msg_queue->tail + 1) & (msg_queue->size - 1);
    log_info(4, "%s current tail: %u thread write index: %u", queue,
             msg_queue->tail, slot_index);
    slot_t *slot = &(msg_queue->queue[slot_index]);
    memcpy(&(slot->msg), msg, sizeof(jia_msg_t)); // copy msg to slot
    slot->state = SLOT_BUSY;                      // set slot state to busy

    sem_post(&(msg_queue->busy_count));
    sem_getvalue(&msg_queue->busy_count, &semvalue);
    log_info(4, "after %s enqueue busy_count value: %d", queue, semvalue);
    // unsigned old_value = msg_queue->busy_count;
    // msg_queue->busy_count++;
    // log_info(4, "after %s enqueue busy_count value: %d", queue, old_value);

    pthread_mutex_unlock(&(msg_queue->tail_lock));
    return 0;
}

int dequeue(msg_queue_t *msg_queue, jia_msg_t *msg) {
    unsigned current_value;
    unsigned slot_index;
    if (msg_queue == NULL || msg == NULL) {
        return -1;
    }
    char *queue = (msg_queue == &outqueue) ? "outqueue" : "inqueue";

    // sem or atomic instruction
    // log_info(4, "pre %s dequeue busy_count value: %d", queue,
    //          msg_queue->busy_count);
    int semvalue;
    sem_getvalue(&msg_queue->busy_count, &semvalue);
    log_info(4, "pre %s dequeue busy_count value: %d", queue, semvalue);
    // wait for busy slot
    if (sem_wait(&msg_queue->busy_count) != 0) {
        return -1;
    }
    sem_getvalue(&msg_queue->busy_count, &semvalue);
    log_info(4, "enter %s dequeue! busy_count value: %d", queue, semvalue);
    /**
     * step 1: get current value
     * step 2: we will atomic sub busy_count iff current value > 0
     */
    // while (1) {
    //     current_value = atomic_load(&msg_queue->busy_count);
    //     if (current_value > 0) {
    //         if (atomic_compare_exchange_weak(&msg_queue->busy_count,
    //                                          &current_value,
    //                                          current_value - 1)) {
    //             break;
    //         }
    //     }
    // }
    // log_info(4, "enter %s dequeue! busy_count value: %d", queue,
    //          current_value - 1);

    // lock head and update head pointer
    pthread_mutex_lock(&(msg_queue->head_lock));
    slot_index = msg_queue->head;
    msg_queue->head = (msg_queue->head + 1) & (msg_queue->size - 1);
    log_info(4, "%s current head: %u thread write index: %u", queue,
             msg_queue->head, slot_index);
    slot_t *slot = &(msg_queue->queue[slot_index]);
    memcpy(msg, &(slot->msg), sizeof(jia_msg_t)); // copy msg from slot
    slot->state = SLOT_FREE;                      // set slot state to free

    sem_post(&(msg_queue->free_count));
    sem_getvalue(&msg_queue->free_count, &semvalue);
    log_info(4, "after %s dequeue free_count value: %d", queue, semvalue);

    // unsigned old_value = msg_queue->free_count;
    // msg_queue->free_count++;
    // log_info(4, "after %s dequeue free_count value: %d", queue, old_value + 1);

    pthread_mutex_unlock(&(msg_queue->head_lock));

    return 0;
}

void free_msg_queue(msg_queue_t *msg_queue) {
    if (msg_queue == NULL) {
        return;
    }

    // destory semaphores
    sem_destroy(&(msg_queue->busy_count));
    sem_destroy(&(msg_queue->free_count));

    // destory head mutex and tail mutex
    pthread_mutex_destroy(&(msg_queue->head_lock));
    pthread_mutex_destroy(&(msg_queue->tail_lock));

    free(msg_queue->queue);
}

/**
 * @brief init_comm_manager - initialize comm_manager
 *
 * @return int
 */
static int init_comm_manager() {
    /** step 1:set snd, rcv, ack ports */
    // snd port: Port monitored by peer host i
    // rcv port: Port monitored by local host that will be used by peer host i
    comm_manager.snd_server_port = start_port + system_setting.jia_pid;
    comm_manager.ack_port = start_port + Maxhosts;
    for (int i = 0; i < Maxhosts; i++) {
        comm_manager.rcv_ports[i] = start_port + i;
    }

    /** step 2:bind fds */
    for (int i = 0; i < Maxhosts; i++) {
        // create socket and bind it to [INADDR_ANY, comm_manager.rcv_ports[i]
        // request from (host i) is will be receive from commreq.rcv_fds[i]
        // (whose port = comm_manager.rcv_ports[i])
        comm_manager.rcv_fds[i] = fd_create(i, FDCR_RECV);
        set_nonblocking(comm_manager.rcv_fds[i]);
    }
    // snd_fds socket fd with random port
    comm_manager.snd_fds = fd_create(0, FDCR_SEND);
    // ack_fds socket fd with ack port
    comm_manager.ack_fds = fd_create(0, FDCR_ACK);
    set_nonblocking(comm_manager.ack_fds);

    /** step 3:init seq */
    for (int i = 0; i < Maxhosts; i++) {
        comm_manager.snd_seq[i] = 0;
        comm_manager.ack_seq[i] = 0;
        comm_manager.rcv_seq[i] = 0;
    }

    return 0;
}

#else  /* NULL_LIB */
#endif /* NULL_LIB */
