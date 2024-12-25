#include "rdma_comm.h"
#include "tools.h"
#include <infiniband/verbs.h>
#include <pthread.h>
#include <semaphore.h>

pthread_t rdma_listen_tid;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
static struct ibv_recv_wr *bad_wr = NULL;
static struct ibv_wc wc;
struct ibv_sge sge_list[20];
struct ibv_recv_wr wr_list[50];

int post_recv() {
    for (int i = 0; i < ctx.batching_num; i++) {
        sge_list[i].addr = (uint64_t)&ctx.inqueue->queue[ctx.inqueue->tail].msg;
        sge_list[i].length = sizeof(jia_msg_t);
        sge_list[i].lkey = ctx.recv_mr[ctx.inqueue->tail]->lkey;
        wr_list[i].sg_list = &sge_list[i];
        wr_list[i].num_sge = 1;
        ctx.inqueue->tail = (ctx.inqueue->tail + 1) % ctx.inqueue->size;
    }

    for (int i = 0; i < ctx.batching_num - 1; i++) {
        wr_list[i].next = &wr_list[i + 1];
    }

    int ret = ibv_post_recv(ctx.qp, &wr_list[0], &bad_wr);
    if (ret) {
        log_err("Failed to post recv");
        return -1;
    }
    return 0;
}

void *rdma_listen(void *arg) {
    pthread_mutex_lock(&ctx.inqueue->lock);

    int value;
    int num_completions = 0;
    sem_getvalue(&ctx.inqueue->free_count, &value);
    while (value < ctx.batching_num) {
        pthread_cond_wait(&cond, &ctx.inqueue->lock);
    }

    while (post_recv())
        ;

    while (num_completions < ctx.batching_num) {
        int nc = ibv_poll_cq(ctx.recv_cq, 1, &wc);
        if (nc < 0) {
            log_err("ibv_poll_cq failed");
            continue;
        }

        if (wc.status != IBV_WC_SUCCESS) {
            log_err("Failed status %s (%d) for wr_id %d", ibv_wc_status_str(wc.status), wc.status, (int)wc.wr_id);
            continue;
        } else {
            ctx.inqueue->queue[ctx.inqueue->tail].state = SLOT_BUSY;
        }
        ctx.inqueue->tail = (ctx.inqueue->tail + 1) % ctx.inqueue->size;
        num_completions++;
    }
    pthread_mutex_unlock(&ctx.inqueue->lock);
}