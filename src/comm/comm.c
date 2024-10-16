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
 **********************************************************************/

#include "global.h"
#include "msg.h"
#ifndef NULL_LIB
#include "comm.h"
#include "stat.h"
#include "tools.h"
#include <pthread.h>
#include <semaphore.h>
#include <stdatomic.h>
#include <sys/socket.h>

long start_port;

// global variables

/* communication manager */
comm_manager_t comm_manager;

/* in/out queue */
msg_queue_t inqueue;
msg_queue_t outqueue;

static int init_comm_manager();
static int fd_create(int i, enum FDCR_MODE flag);
static void sigint_handler();
static void sigio_handler();

// function definitions

/**
 * @brief register_sigint_handler -- register sigint signal handler
 *
 */
void register_sigint_handler() {
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
    int ret;

    /** step 1: allocate memory size for msg_queue */
    if (size <= 0) {
        size = system_setting.msg_queue_size;
    }
//    msg_queue->queue = (slot_t *)malloc(sizeof(slot_t) * size);
    msg_queue->queue = (unsigned char **)malloc(sizeof(unsigned char *) * size);

    for (int i = 0; i < size; i++) {
        ret = posix_memalign((void **)&msg_queue->queue[i], Pagesize, Maxsize);
        if (ret != 0) {
            fprintf(stderr, "Allocated queue failed!\n");
            exit(-1);
        }
    }

    if (msg_queue->queue == NULL) {
        perror("msg_queue malloc");
        return -1;
    }

    /** step 2: init msg_queue */
    msg_queue->size = size;
    msg_queue->head = 0;
    msg_queue->tail = 0;
    msg_queue->post = 0;
    atomic_init(&msg_queue->busy_value, 0);
    atomic_init(&msg_queue->post_value, 0);
    atomic_init(&msg_queue->free_value, size);


    /** step 3: initialize head mutex and tail mutex */
    if (pthread_mutex_init(&(msg_queue->head_lock), NULL) != 0 ||
        pthread_mutex_init(&(msg_queue->tail_lock), NULL) != 0 ||
        pthread_mutex_init(&(msg_queue->post_lock), NULL) != 0) {
        perror("msg_queue mutex init");
        goto mutex_fail;
    }

    /** step 4: initialize semaphores (or atomic count)*/
    if (sem_init(&(msg_queue->busy_count), 0, 0) != 0 ||
        sem_init(&(msg_queue->free_count), 0, size) != 0) {
        perror("msg_queue sem init");
        goto sem_fail;
    }

    return 0;

sem_fail:
    pthread_mutex_destroy(&(msg_queue->head_lock));
    pthread_mutex_destroy(&(msg_queue->tail_lock));
    pthread_mutex_destroy(&(msg_queue->post_lock));
mutex_fail:
    free(msg_queue->queue);

    return -1;
}

int enqueue(msg_queue_t *msg_queue, jia_msg_t *msg) {
    unsigned current_value;
    unsigned slot_index;

    /* step 0: ensure which queue */
    if (msg_queue == NULL || msg == NULL) {
        log_err("msg_queue or msg is NULL[msg_queue: %lx msg: %lx]",
                (long unsigned)msg_queue, (long unsigned)msg);
        return -1;
    }
    char *queue = (msg_queue == &outqueue) ? "outqueue" : "inqueue";

    /* step 1: sem wait for free slot and print sem value */
    int semvalue;
    sem_getvalue(&msg_queue->free_count, &semvalue);
    log_info(4, "pre %s enqueue free_count value: %d", queue, semvalue);
    if (sem_wait(&msg_queue->free_count) != 0) {
        log_err("sem_wait error");
        return -1;
    }
    sem_getvalue(&msg_queue->free_count, &semvalue);
    log_info(4, "enter %s enqueue! free_count value: %d", queue, semvalue);

    /* step 2: lock tail */
    pthread_mutex_lock(&(msg_queue->tail_lock));

    {
        /* step 2.1: update tail pointer and memcpy */
        slot_index = msg_queue->tail;
        msg_queue->tail = (msg_queue->tail + 1) & (msg_queue->size - 1);
        log_info(4, "%s current tail: %u thread write index: %u", queue,
                 msg_queue->tail, slot_index);

        memcpy(msg_queue->queue[slot_index], msg, sizeof(jia_msg_t)); // copy msg to slot

        /* step 2.2: sem post busy count */
        sem_post(&(msg_queue->busy_count));
        sem_getvalue(&msg_queue->busy_count, &semvalue);
        log_info(4, "after %s enqueue busy_count value: %d", queue, semvalue);
    }

    /* step 3: unlock tail */
    pthread_mutex_unlock(&(msg_queue->tail_lock));
    return 0;
}

int dequeue(msg_queue_t *msg_queue, jia_msg_t *msg) {
    unsigned current_value;
    unsigned slot_index;

    /* step 0: ensure which queue */
    if (msg_queue == NULL || msg == NULL) {
        return -1;
    }
    char *queue = (msg_queue == &outqueue) ? "outqueue" : "inqueue";

    /* step 1: sem wait for busy slot and print sem value */
    int semvalue;
    sem_getvalue(&msg_queue->busy_count, &semvalue);
    log_info(4, "pre %s dequeue busy_count value: %d", queue, semvalue);
    if (sem_wait(&msg_queue->busy_count) != 0) {
        return -1;
    }
    sem_getvalue(&msg_queue->busy_count, &semvalue);
    log_info(4, "enter %s dequeue! busy_count value: %d", queue, semvalue);

    /* step 2: lock head */
    pthread_mutex_lock(&(msg_queue->head_lock));

    {
        /* step 2.1: update head pointer and memcpy */
        slot_index = msg_queue->head;
        msg_queue->head = (msg_queue->head + 1) & (msg_queue->size - 1);
        log_info(4, "%s current head: %u thread write index: %u", queue,
                 msg_queue->head, slot_index);

        memcpy(msg, msg_queue->queue[slot_index], sizeof(jia_msg_t)); // copy msg from slot

        /* step 2.2: sem post free count */
        sem_post(&(msg_queue->free_count));
        sem_getvalue(&msg_queue->free_count, &semvalue);
        log_info(4, "after %s dequeue free_count value: %d", queue, semvalue);
    }

    /* step 3: unlock head */
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
    pthread_mutex_destroy(&(msg_queue->post_lock));

    // free the queue space
    for (int i = 0; i < msg_queue->size; i++) {
        free(msg_queue->queue[i]);
    }


    free(msg_queue->queue);
}

#else  /* NULL_LIB */
#endif /* NULL_LIB */
