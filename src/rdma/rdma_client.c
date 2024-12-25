#include "rdma_comm.h"
#include "tools.h"
#include <pthread.h>
#include <semaphore.h>

pthread_t rdma_client_tid;
static struct ibv_wc wc;
static struct ibv_send_wr *bad_wr;
int seq = 0;

int post_send(jia_context_t *ctx) {
    jia_msg_t *msg_ptr = &ctx->outqueue->queue[ctx->outqueue->head].msg;
    struct ibv_sge sge = {
        .addr = (uint64_t)msg_ptr,
       .length = sizeof(jia_msg_t),
      .lkey = ctx->send_mr[ctx->outqueue->head]->lkey
    };

    struct ibv_send_wr wr = {
        .wr_id = seq,
        .sg_list = &sge,
        .num_sge = 1,
        .opcode = IBV_WR_SEND,
        .send_flags = IBV_SEND_SIGNALED,
        .wr = {
            .ud = {
                .ah = ctx->ah[msg_ptr->topid],
                .remote_qpn = dest_info[msg_ptr->topid].qpn,
                .remote_qkey = 0x11111111
            }
        }
    };

    if (ibv_post_send(ctx->qp, &wr, &bad_wr)) {
        log_err("Failed to post send");
        return -1;
    }
    seq++;
    return 0;
}


void *rdma_client(void *arg) {
    while (1) {
        sem_wait(&ctx.outqueue->busy_count);
        int ne;
    label1:
        while(post_send(&ctx))
            ;
        ne = ibv_poll_cq(ctx.send_cq, 1, &wc);
        if (ne < 0) {
            log_err("ibv_poll_cq failed");
        }
        if (wc.status != IBV_WC_SUCCESS) {
            log_err("Failed status %s (%d) for wr_id %d", ibv_wc_status_str(wc.status), wc.status, (int)wc.wr_id);
            goto label1;
        }
        log_info(3, "Send outqueue[%d] msg successfully", ctx.outqueue->head);
        ctx.outqueue->head = (ctx.outqueue->head + 1) % system_setting.msg_queue_size;
        sem_post(&ctx.outqueue->free_count);
    }
}
