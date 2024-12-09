#ifndef RDMA_COMM_H
#define RDMA_COMM_H

#include <infiniband/verbs.h>

struct context {
    struct ibv_context *ctx;
    struct ibv_pd *pd;
    struct ibv_cq *cq;
    struct ibv_qp *qp;
    struct ibv_mr *mr;
    char *buf;
    int buf_size;
    int rx_depth;
};

#define CQ_SIZE 32

static struct context *create_context(struct ibv_device *ib_dev, int size, int rx_depth);
static int post_recv(struct context *ctx, int n);
static int post_send(struct context *ctx, struct ibv_ah *ah, uint32_t remote_qpn, struct message *msg);


#endif //!RDMA_COMM_H