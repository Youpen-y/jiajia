#ifndef RDMA_H
#define RDMA_H

#include "msg.h"
#include "global.h"
#include <infiniband/verbs.h>
#include <rdma/rdma_cma.h>
#include <pthread.h>
#include <stdbool.h>

#define BatchingSize 1
#define QueueSize 32
#define BatchingNum (QueueSize / BatchingSize)

typedef struct rdma_connect {
    bool connected;
    unsigned    snd_seq;
    unsigned    rcv_seq;
    struct rdma_cm_id id;

    struct ibv_mr **in_mr;
    msg_queue_t *inqueue;
} rdma_connect_t;

typedef struct jia_context {
    struct ibv_context *context;
    struct ibv_pd *pd;                      // common pd
    struct ibv_comp_channel *recv_comp_channel;  // common recv io completion channel
    struct ibv_comp_channel *send_comp_channel;

    // port related
    int ib_port;                   // ib port number

    // info data
    int batching_num; // post recv wr doorbell batching num

    // rdma connect (Maxhosts inqueues, only one outqueue)
    msg_queue_t *outqueue;
    struct ibv_mr *out_mr[QueueSize];
    rdma_connect_t connect_array[Maxhosts];

    // connection parameters
    pthread_t server_thread;   // server thread for connection from client on other hosts
    pthread_t *client_threads; // client threads used to connect other hosts
} jia_context_t;

extern jia_context_t ctx;

extern pthread_t rdma_client_tid;
extern pthread_t rdma_listen_tid;
extern pthread_t rdma_server_tid;

/* function declaration */

/**
 * @brief init_rdma_comm -- init rdma communication
 * 
 */
void init_rdma_comm();

/**
 * @brief rdma_listen_thread -- rdma listen thread (post recv wr by doorbell batching)
 */
void *rdma_listen_thread(void *arg);

/**
 * @brief rdma_client_thread -- rdma client thread (post send wr)
 */
void *rdma_client_thread(void *arg);

/**
 * @brief rdma_server_thread -- rdma server thread (handle msg)
 */
void *rdma_server_thread(void *arg);

/**
 * @brief free_rdma_resources -- free rdma related resources
 * @param ctx -- jiajia rdma context
 */
void free_rdma_resources(struct jia_context *ctx);
#endif