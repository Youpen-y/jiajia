#include "msg.h"
#include "rdma.h"
#include "setting.h"
#include "tools.h"
#include <infiniband/verbs.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdatomic.h>
#include <stdio.h>

#define min(a, b) ((a) < (b) ? (a) : (b))

pthread_cond_t cond_server = PTHREAD_COND_INITIALIZER;
pthread_mutex_t lock_server = PTHREAD_MUTEX_INITIALIZER;
pthread_t rdma_server_tid;
jia_msg_t msg;
struct ibv_sge sge_list[QueueSize * Maxhosts];
struct ibv_recv_wr wr_list[QueueSize * Maxhosts];
static struct ibv_recv_wr *bad_wr = NULL;

extern rdma_connect_t connect_array[Maxhosts];

void prepare_recv_wr(struct ibv_mr **mr, unsigned index);
int init_post_recv_wr();

void *rdma_server_thread(void *arg) {
    int ret;
    if ((ret = init_post_recv_wr())) {   // post recv wr first
        log_err("init recv wr error");
    }

    while (1) {
        /* step 1: lock and enter inqueue to check if busy slot number is
         * greater than ctx.batching_num */
        // pthread_mutex_lock(&lock_server);

        for (int i = 0; i < Maxhosts; i = (i + 1) % Maxhosts) {
            /* get connect and inqueue */
            rdma_connect_t *tmp_connect = &ctx.connect_array[i];
            msg_queue_t *inqueue = tmp_connect->inqueue;

            if (i == system_setting.jia_pid)
                continue;
            if (atomic_load(&(ctx.connect_array[i].inqueue->busy_value)) > 0) {

                /* step 1: handle msg and update head point, busy_value */
                msg_handle((jia_msg_t *)(inqueue->queue[inqueue->head]));

                log_info(3, "pre inqueue [head]: %d, [busy_value]: %d [free_value]: %d",
                         inqueue->head, atomic_load(&inqueue->busy_value), atomic_load(&inqueue->free_value));

                /* step 2: sub busy_value and add free_value */
                if (atomic_load(&(inqueue->busy_value)) <= 0) {
                    log_err("busy value error <= 0");
                } else {
                    atomic_fetch_sub(&(inqueue->busy_value), 1);
                }
                atomic_fetch_add(&(inqueue->free_value), 1);

                /* step 2: update head */
                pthread_mutex_lock(&inqueue->head_lock);
                inqueue->head = (inqueue->head + 1) % inqueue->size;
                pthread_mutex_unlock(&inqueue->head_lock);

                log_info(3, "after inqueue [head]: %d, [busy_value]: %d [free_value]: %d",
                         inqueue->head, atomic_load(&inqueue->busy_value), atomic_load(&inqueue->free_value));
            }
            check_flags(i);
        }
    }
}

/** init BatchingSize num recv_wr */
void prepare_recv_wr(struct ibv_mr **mr, unsigned index) {
    unsigned limit = min((BatchingSize), ((index / QueueSize) + 1) * QueueSize - index);
    /* 在当前这个id对应的wr_list中的序号 */
    unsigned rindex = (index % QueueSize);

    /* step 1: init sge_list and wr_list */
    for (int i = 0; i < limit; i++) {
        sge_list[index + i].addr = (uint64_t)mr[rindex + i]->addr;
        sge_list[index + i].length = (uint32_t)mr[rindex + i]->length;
        sge_list[index + i].lkey = mr[rindex + i]->lkey;
        /* now we link to the send work request */
        bzero(&wr_list[index + i], sizeof(struct ibv_recv_wr));
        wr_list[index + i].sg_list = &sge_list[index + i];
        wr_list[index + i].num_sge = 1;
    }

    /* step 2: make wr_list link array */
    int i;
    for (i = 0; i < limit - 1; i++) {
        wr_list[index + i].next = &wr_list[index + i + 1];
    }
    wr_list[index + i].next = NULL;
}

int check_flags(unsigned cqid) {
    msg_queue_t *inqueue = ctx.connect_array[cqid].inqueue;

    if (atomic_load(&(inqueue->free_value)) >= BatchingSize) {
        log_info(3, "pre inqueue [post]: %d, [free_value]: %d [post_value]: %d", inqueue->post,
                 atomic_load(&inqueue->free_value), atomic_load(&inqueue->post_value));

        /* step 1: new BatchingSize post recv */
        prepare_recv_wr(ctx.connect_array[cqid].in_mr, inqueue->post + cqid * QueueSize);
        while (ibv_post_recv(ctx.connect_array[cqid].id.qp,
                             &wr_list[inqueue->post + cqid * QueueSize], &bad_wr)) {
            log_err("Failed to post recv");
        };

        /* step 2: add free_value and sub post_value */
        atomic_fetch_sub(&(inqueue->free_value), BatchingSize);
        atomic_fetch_add(&(inqueue->post_value), BatchingSize);

        /* step 3: update inqueue->post */
        pthread_mutex_lock(&inqueue->post_lock);
        inqueue->post = (inqueue->post + BatchingSize) % QueueSize;
        pthread_mutex_unlock(&inqueue->post_lock);

        log_info(3, "after inqueue [post]: %d, [free_value]: %d [post_value]: %d", inqueue->post,
                 atomic_load(&inqueue->free_value), atomic_load(&inqueue->post_value));
    }
    return 0;
}

int init_post_recv_wr() {
    int ret;

    /* step 1: init wr, sge, for rdma to recv */
    for (int j = 0; j < Maxhosts; j++) {
        if (j == system_setting.jia_pid)
            continue;
        for (int i = 0; i < QueueSize; i += BatchingSize) {
            prepare_recv_wr(ctx.connect_array[j].in_mr, j * QueueSize + i);
        }
    }

    /* step 2: post recv wr */
    for (int j = 0; j < Maxhosts; j++) {
        if (j == system_setting.jia_pid)
            continue;

        msg_queue_t *inqueue = ctx.connect_array[j].inqueue;
        for (int i = 0; i < QueueSize; i += BatchingSize) {
            /* step 1: loop until ibv_post_recv wr successfully */
            while (
                (ret = ibv_post_recv(ctx.connect_array[j].id.qp, &wr_list[i + j * QueueSize], &bad_wr))) {
                log_err("Failed to post recv");
            }

            /* step 2: add free_value and sub post_value */
            atomic_fetch_sub(&(inqueue->free_value), BatchingSize);
            atomic_fetch_add(&(inqueue->post_value), BatchingSize);

            /* step 3: update inqueue->post */
            pthread_mutex_lock(&ctx.connect_array[j].inqueue->post_lock);
            inqueue->post = (inqueue->post + BatchingSize) % QueueSize;
            pthread_mutex_unlock(&ctx.connect_array[j].inqueue->post_lock);
        }
    }

    return 0;
}