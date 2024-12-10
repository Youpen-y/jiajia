#ifndef NULL_LIB

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "rdma_comm.h"

static struct context *create_context(struct ibv_device *ib_dev, int size, int rx_depth) {
   struct context *ctx;

    ctx = calloc(1, sizeof *ctx);
    if (!ctx)
        return NULL;

    ctx->size = size;
    ctx->rx_depth = rx_depth;
    ctx->buf = malloc(size);
    if (!ctx->buf) {
        fprintf(stderr, "Couldn't allocate work buf.\n");
        goto clean_ctx;
    }

    ctx->ctx = ibv_open_device(ib_dev);
    if (!ctx->ctx) {
        fprintf(stderr, "Couldn't get context for %s\n", ibv_get_device_name(ib_dev));
        goto clean_buffer;
    }

    ctx->pd = ibv_alloc_pd(ctx->ctx);
    if (!ctx->pd) {
        fprintf(stderr, "Couldn't allocate PD\n");
        goto clean_device;
    }

    ctx->mr = ibv_reg_mr(ctx->pd, ctx->buf, size, IBV_ACCESS_LOCAL_WRITE);
    if (!ctx->mr) {
        fprintf(stderr, "Couldn't register MR\n");
        goto clean_pd;
    }

    ctx->cq = ibv_create_cq(ctx->ctx, CQ_SIZE, NULL, NULL, 0);
    if (!ctx->cq) {
        fprintf(stderr, "Couldn't create CQ\n");
        goto clean_mr;
    }

    {
        struct ibv_qp_init_attr attr = {
            .send_cq = ctx->cq,
            .recv_cq = ctx->cq,
            .cap     = {
                .max_send_wr  = 1,
                .max_recv_wr  = rx_depth,
                .max_send_sge = 1,
                .max_recv_sge = 1
            },
            .qp_type = IBV_QPT_UD,
        };

        ctx->qp = ibv_create_qp(ctx->pd, &attr);
        if (!ctx->qp)  {
            fprintf(stderr, "Couldn't create QP\n");
            goto clean_cq;
        }
    }

    {
        struct ibv_qp_attr attr = {
            .qp_state        = IBV_QPS_INIT,
            .pkey_index      = 0,
            .port_num        = 1,
            .qkey            = 0x11111111
        };

        if (ibv_modify_qp(ctx->qp, &attr,
                IBV_QP_STATE              |
                IBV_QP_PKEY_INDEX         |
                IBV_QP_PORT               |
                IBV_QP_QKEY)) {
            fprintf(stderr, "Failed to modify QP to INIT\n");
            goto clean_qp;
        }
    }

    return ctx;

clean_qp:
    ibv_destroy_qp(ctx->qp);

clean_cq:
    ibv_destroy_cq(ctx->cq);

clean_mr:
    ibv_dereg_mr(ctx->mr);

clean_pd:
    ibv_dealloc_pd(ctx->pd);

clean_device:
    ibv_close_device(ctx->ctx);

clean_buffer:
    free(ctx->buf);

clean_ctx:
    free(ctx);

    return NULL;
}

static int post_recv(struct context *ctx, int n)
{
    struct ibv_sge list = {
        .addr   = (uintptr_t) ctx->buf,
        .length = ctx->size,
        .lkey   = ctx->mr->lkey
    };
    struct ibv_recv_wr wr = {
        .wr_id      = 0,
        .sg_list    = &list,
        .num_sge    = 1,
    };
    struct ibv_recv_wr *bad_wr;
    int i;

    for (i = 0; i < n; ++i) {
        if (ibv_post_recv(ctx->qp, &wr, &bad_wr))
            break;
    }

    return i;
}

static int post_send(struct context *ctx, struct ibv_ah *ah, uint32_t remote_qpn, struct message *msg)
{
    struct ibv_sge list = {
        .addr   = (uintptr_t) msg,
        .length = sizeof(*msg),
        .lkey   = ctx->mr->lkey
    };
    struct ibv_send_wr wr = {
        .wr_id      = 0,
        .sg_list    = &list,
        .num_sge    = 1,
        .opcode     = IBV_WR_SEND,
        .send_flags = IBV_SEND_SIGNALED,
        .wr         = {
            .ud = {
                 .ah          = ah,
                 .remote_qpn  = remote_qpn,
                 .remote_qkey = 0x11111111
             }
        }
    };
    struct ibv_send_wr *bad_wr;

    return ibv_post_send(ctx->qp, &wr, &bad_wr);
}


#endif