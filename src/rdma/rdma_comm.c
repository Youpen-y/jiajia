#include "rdma.h"
#include "msg.h"
#include "setting.h"
#include "tools.h"
#include <arpa/inet.h>
#include <endian.h>
#include <infiniband/verbs.h>
#include <pthread.h>
#include <rdma/rdma_cma.h>
#include <rdma/rdma_verbs.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define Msgsize 200    // temp value, used inlined message size in rdma
#define MAX_RETRY 1000 // client's max retry times to connect server

extern long start_port;
pthread_mutex_t recv_comp_channel_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t send_comp_channel_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t pd_mutex = PTHREAD_MUTEX_INITIALIZER;
jia_context_t ctx;
static void sync_connection(int total_hosts, int jia_pid);

void init_rdma_comm() {
    int i, j, fd;

    if (system_setting.jia_pid == 0) {
        VERBOSE_LOG(3, "************Initialize RDMA Communication!*******\n");
    }
    VERBOSE_LOG(3, "current jia_pid = %d\n", system_setting.jia_pid);
    VERBOSE_LOG(3, " start_port = %ld \n", start_port);


    /* step 1: init rdma context */
    init_rdma_context(&ctx, BatchingSize);

    /* step 2: establish connections between hosts*/
    sync_connection(system_setting.hostc, system_setting.jia_pid);

    /* step 3: init rdma resource */
    init_rdma_resource(&ctx);

    /* step 4: create rdma listen, client, server thread */
    pthread_create(&rdma_listen_tid, NULL, rdma_listen_thread, NULL);
    pthread_create(&rdma_client_tid, NULL, rdma_client_thread, NULL);
    pthread_create(&rdma_server_tid, NULL, rdma_server_thread, NULL);

    /* step 5: register sigint handler */
    register_sigint_handler();
}

void *sync_server_thread(void *arg) {
    struct rdma_cm_id *listener = NULL;
    struct rdma_event_channel *ec = NULL;
    struct sockaddr_in addr;
    struct rdma_cm_event *event = NULL;
    struct ibv_qp_init_attr qp_attr;
    struct rdma_conn_param conn_param; // connect parameters that transfer to client in rdma_accept

    int ret;
    int completion_num = 0; // completed connection number
    int client_id;          // used to store the client id that try to connect the server

    /** step 1: create event channel */
    ec = rdma_create_event_channel();
    if (!ec) {
        fprintf(stderr, "Host %d: Failed to create event channel\n", system_setting.jia_pid);
        return NULL;
    }

    /** step 2: create listen rdma_cm_id */
    ret = rdma_create_id(ec, &listener, NULL, RDMA_PS_TCP);
    if (ret) {
        fprintf(stderr, "Host %d: Failed to create RDMA CM ID\n", system_setting.jia_pid);
        goto cleanup;
    }

    /** step 3: bind to server's ip addr */
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(start_port + system_setting.jia_pid);
    addr.sin_addr.s_addr = inet_addr(system_setting.hosts[system_setting.jia_pid].ip);
    ret = rdma_bind_addr(listener, (struct sockaddr *)&addr);
    if (ret) {
        fprintf(stderr, "Host %d: Failed to bind address\n", system_setting.jia_pid);
        goto cleanup;
    }

    /** step 4: listen... */
    ret = rdma_listen(listener, Maxhosts);
    if (ret) {
        fprintf(stderr, "Host %d: Failed to listen\n", system_setting.jia_pid);
        goto cleanup;
    }

    printf("Host %d: Server listening on port %ld\n", system_setting.jia_pid, start_port + system_setting.jia_pid);

    while (1) {
        // by default, the following call will block until an event is received
        ret = rdma_get_cm_event(ec, &event);
        if (ret) {
            continue;
        }

        switch (event->event) {
        case RDMA_CM_EVENT_CONNECT_REQUEST:
            // get the private data(i.e. id) from client
            client_id = *(int *)event->param.conn.private_data;
            log_out(4, "Received Connect Request from host %d\n", client_id);

            // if the common completion channel is null, create it, otherwise use it directly
            pthread_mutex_lock(&recv_comp_channel_mutex);
            if (ctx.recv_comp_channel == NULL)
                ctx.recv_comp_channel = ibv_create_comp_channel(event->id->verbs);
            pthread_mutex_unlock(&recv_comp_channel_mutex);

            pthread_mutex_lock(&send_comp_channel_mutex);
            if (ctx.send_comp_channel == NULL)
                ctx.send_comp_channel = ibv_create_comp_channel(event->id->verbs);
            pthread_mutex_unlock(&send_comp_channel_mutex);

            // set QP attributes
            memset(&qp_attr, 0, sizeof(qp_attr));
            qp_attr.qp_context = NULL;
            qp_attr.cap.max_send_wr = QueueSize * 4;
            qp_attr.cap.max_recv_wr = QueueSize * 4;
            qp_attr.cap.max_send_sge = 1;
            qp_attr.cap.max_recv_sge = 1;
            qp_attr.cap.max_inline_data = 88;
            qp_attr.qp_type = IBV_QPT_RC;
            // there, we use ctx.connect_array[client_id] as cq_context, which will be catched by
            // ibv_get_cq_event
            qp_attr.send_cq =
                ibv_create_cq(event->id->verbs, QueueSize, &(ctx.connect_array[client_id]),
                              ctx.send_comp_channel, 0);
            qp_attr.recv_cq =
                ibv_create_cq(event->id->verbs, QueueSize, &(ctx.connect_array[client_id]),
                              ctx.recv_comp_channel, 0);

            // request completion notification on send_cq and recv_cq
            ibv_req_notify_cq(qp_attr.send_cq, 0);
            ibv_req_notify_cq(qp_attr.recv_cq, 0);

            // if the pd is not null, use it, otherwise create it and use
            pthread_mutex_lock(&pd_mutex);
            if (ctx.pd == NULL) {
                ctx.pd = ibv_alloc_pd(event->id->verbs);
            }
            pthread_mutex_unlock(&pd_mutex);

            // create QP
            ret = rdma_create_qp(event->id, ctx.pd, &qp_attr);
            if (!ret) {
                // there, server can transfer its private data to client too
                memset(&conn_param, 0, sizeof(conn_param));
                conn_param.private_data = &(system_setting.jia_pid);
                conn_param.private_data_len = sizeof(system_setting.jia_pid);

                // conn_param.responder_resources = 4;
                // conn_param.initiator_depth = 1;
                // conn_param.rnr_retry_count = 7;
                conn_param.responder_resources = 8; // 可同时处理 2 个 RDMA Read
                conn_param.initiator_depth = 8;     // 可以发起 2 个并发的 RDMA Read
                conn_param.flow_control = 1;        // 启用流控
                conn_param.retry_count = 7;         // 发送重传 7 次
                conn_param.rnr_retry_count = 7;     // RNR 重传 7 次

                // don't use SRQ, use QP num that allocated by system
                conn_param.srq = 0;
                conn_param.qp_num = 0;

                // accept the connect
                ret = rdma_accept(event->id, &conn_param);
                if (!ret) {
                    printf("Host %d: Accepted connection\n", system_setting.jia_pid);
                }
            }
            break;

        case RDMA_CM_EVENT_ESTABLISHED:
            log_info(3, "Host %d: Connection established\n", system_setting.jia_pid);

            // update the connection status
            ctx.connect_array[client_id].connected = true;
            ctx.connect_array[client_id].id = *(event->id);
            completion_num++;
            if (completion_num == system_setting.jia_pid) {
                // server has finished its job
                goto cleanup;
            }
            break;

        case RDMA_CM_EVENT_DISCONNECTED:
            rdma_destroy_qp(event->id);
            rdma_destroy_id(event->id);
            printf("Host %d: Connection disconnected\n", system_setting.jia_pid);
            break;

        default:
            break;
        }

        rdma_ack_cm_event(event);
    }

cleanup:
    if (listener) {
        rdma_destroy_id(listener);
    }
    if (ec) {
        rdma_destroy_event_channel(ec);
    }
    return NULL;
}

void *sync_client_thread(void *arg) {
    int server_id = *(int *)arg;
    struct rdma_cm_id *id = NULL;
    struct rdma_event_channel *ec = NULL;
    struct sockaddr_in addr;
    struct rdma_cm_event *event = NULL;
    struct ibv_qp_init_attr qp_attr;
    struct rdma_conn_param conn_param;

    int ret;
    bool retry_flag = true;
    int retry_count = 0;

    /** step 1: create event channel */
    ec = rdma_create_event_channel();
    if (!ec) {
        fprintf(stderr, "Host %d: Failed to create event channel for client[%d]\n", system_setting.jia_pid,
                server_id);
        return NULL;
    }

    while (retry_flag && retry_count < MAX_RETRY) {
        retry_flag = false;

        /** step 2: create listen rdma_cm_id */
        ret = rdma_create_id(ec, &id, NULL, RDMA_PS_TCP);
        if (ret) {
            fprintf(stderr, "Host %d: Failed to create RDMA CM ID for client[%d]\n", system_setting.jia_pid,
                    server_id);
            goto cleanup;
        }

        /** step 3: resolve the dest's addr */
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(start_port + server_id);
        addr.sin_addr.s_addr = inet_addr(system_setting.hosts[server_id].ip);
        ret = rdma_resolve_addr(id, NULL, (struct sockaddr *)&addr, 2000);
        if (ret) {
            fprintf(stderr, "Host %d: Failed to resolve address for host %d\n", system_setting.jia_pid, server_id);
            goto cleanup;
        }

        while (1) {
            ret = rdma_get_cm_event(ec, &event);
            if (ret) {
                continue;
            }

            switch (event->event) {
            case RDMA_CM_EVENT_ADDR_RESOLVED: /** step 4: resolve the route */
                ret = rdma_resolve_route(event->id, 2000);
                break;

            case RDMA_CM_EVENT_ROUTE_RESOLVED: /** step 5: initiate a connection */
                pthread_mutex_lock(&recv_comp_channel_mutex);
                if (ctx.recv_comp_channel == NULL)
                    ctx.recv_comp_channel = ibv_create_comp_channel(event->id->verbs);
                pthread_mutex_unlock(&recv_comp_channel_mutex);

                pthread_mutex_lock(&send_comp_channel_mutex);
                if (ctx.send_comp_channel == NULL)
                    ctx.send_comp_channel = ibv_create_comp_channel(event->id->verbs);
                pthread_mutex_unlock(&send_comp_channel_mutex);

                // set QP attribute
                memset(&qp_attr, 0, sizeof(qp_attr));
                qp_attr.qp_context = NULL;
                qp_attr.cap.max_send_wr = QueueSize * 4;
                qp_attr.cap.max_recv_wr = QueueSize * 4;
                qp_attr.cap.max_send_sge = 1;
                qp_attr.cap.max_recv_sge = 1;
                qp_attr.cap.max_inline_data = 64;
                qp_attr.qp_type = IBV_QPT_RC;
                // there, we use ctx.connect_array[client_id] as cq_context, which will be catched
                // by ibv_get_cq_event
                qp_attr.send_cq =
                    ibv_create_cq(event->id->verbs, QueueSize * 4, &(ctx.connect_array[server_id]),
                                  ctx.send_comp_channel, 0);
                qp_attr.recv_cq =
                    ibv_create_cq(event->id->verbs, QueueSize * 4, &(ctx.connect_array[server_id]),
                                  ctx.recv_comp_channel, 0);

                // request completion notification on send_cq and recv_cq
                ibv_req_notify_cq(qp_attr.send_cq, 0);
                ibv_req_notify_cq(qp_attr.recv_cq, 0);

                pthread_mutex_lock(&pd_mutex);
                if (ctx.pd == NULL) {
                    ctx.pd = ibv_alloc_pd(id->verbs);
                }
                pthread_mutex_unlock(&pd_mutex);

                // create QP
                ret = rdma_create_qp(event->id, ctx.pd, &qp_attr);
                if (!ret) {
                    // set private data (there, use id)
                    memset(&conn_param, 0, sizeof(conn_param));
                    conn_param.private_data = &(system_setting.jia_pid);
                    conn_param.private_data_len = sizeof(system_setting.jia_pid);

                    // set resource parameters
                    conn_param.responder_resources = 8; // 可同时处理 2 个 RDMA Read
                    conn_param.initiator_depth = 8;     // 可以发起 2 个并发的 RDMA Read
                    conn_param.flow_control = 1;        // 启用流控
                    conn_param.retry_count = 7;         // 发送重传 7 次
                    conn_param.rnr_retry_count = 7;     // RNR 重传 7 次

                    // don't use SRQ, use QP num that allocated by system
                    conn_param.srq = 0;
                    conn_param.qp_num = 0;

                    ret = rdma_connect(event->id, &conn_param);
                }
                break;

            case RDMA_CM_EVENT_REJECTED: /** step 5.5: connect rejected, reconnect */
                // when client try to connect, but no corresponding server exists, trigger this
                // event
                log_out(4, "[%d]Connect to host %d failed, event: %s", retry_count, server_id,
                        rdma_event_str(event->event));

                // retry or not
                if (retry_count < MAX_RETRY) {
                    retry_count++;
                    retry_flag = true;
                }
                if (id) {
                    rdma_destroy_qp(id);
                }
                id = NULL; // 重置 id
                goto next_try;

            case RDMA_CM_EVENT_ESTABLISHED: /** step 6: connection establishted */
                log_out(4, "\nAfter retried %d connect, Host %d: Connected to host %d\n",
                        retry_count, system_setting.jia_pid, server_id);
                ctx.connect_array[server_id].connected = true;
                ctx.connect_array[server_id].id = *(event->id);

            case RDMA_CM_EVENT_UNREACHABLE:
            case RDMA_CM_EVENT_DISCONNECTED:
                goto cleanup;

            default:
                break;
            }

            rdma_ack_cm_event(event);
        } // while(1)

    next_try:
        if (retry_flag) {}
        continue;
    } // while(retry_flag && retry_count < MAX_RETRY)

cleanup:
    if (ec) {
        rdma_destroy_event_channel(ec);
    }
    return NULL;
}

void init_rdma_context(struct jia_context *ctx, int batching_num) {
    struct ibv_device **dev_list;
    struct ibv_device *dev;

    /* step 1: get device list && get device context */
    dev_list = ibv_get_device_list(NULL);
    if (!dev_list) {
        log_info(3, "No RDMA device available");
    }
    dev = dev_list[0]; // use the first device
    ctx->context = ibv_open_device(dev);
    if (!dev) {
        log_info(3, "Open first device failed");
    }

    /* step 2: init ctx common parameters */
    ctx->ib_port = 2;
    ctx->batching_num = batching_num;
    ctx->outqueue = &outqueue;

    init_msg_queue(ctx->outqueue, QueueSize);

    /* step 3: init rdma connection parameters */
    for (int i = 0; i < system_setting.hostc; i++) {
        ctx->connect_array[i].connected = false;
        ctx->connect_array[i].inqueue = (msg_queue_t *)malloc(sizeof(msg_queue_t));
        init_msg_queue(ctx->connect_array[i].inqueue, QueueSize);
        ctx->connect_array[i].in_mr = (struct ibv_mr **)malloc(sizeof(struct ibv_mr *) * QueueSize);
    }
}

void sync_connection(int total_hosts, int jia_pid) {
    // 创建服务器线程（如果需要）
    if (jia_pid != 0) {
        pthread_create(&ctx.server_thread, NULL, sync_server_thread, &ctx);
    }

    // 创建客户端线程（如果需要）
    int num_clients = total_hosts - jia_pid - 1;
    if (num_clients > 0) {
        ctx.client_threads = malloc(num_clients * sizeof(pthread_t));
        for (int i = 0; i < num_clients; i++) {
            int *target_host = (int *)malloc(sizeof(int));
            *target_host = jia_pid + i + 1;
            pthread_create(&ctx.client_threads[i], NULL, sync_client_thread, target_host);
        }
    }

    // 等待服务器线程
    if (jia_pid != 0) {
        pthread_join(ctx.server_thread, NULL);
    }

    // 等待所有客户端线程
    if (num_clients > 0) {
        for (int i = 0; i < num_clients; i++) {
            pthread_join(ctx.client_threads[i], NULL);
        }
    }
}


void init_rdma_resource(struct jia_context *ctx) {
    /* step 1: register outqueue memory regions */
    for (int i = 0; i < QueueSize; i++) {
        ctx->out_mr[i] =
            ibv_reg_mr(ctx->pd, ctx->outqueue->queue[i], 40960, IBV_ACCESS_LOCAL_WRITE);
    }

    /* step 2: register inqueue memory regions */
    for (int i = 0; i < system_setting.hostc; i++) {
        if (i == system_setting.jia_pid)
            continue;
        for (int j = 0; j < QueueSize; j++) {
            ctx->connect_array[i].in_mr[j] = rdma_reg_msgs(
                &ctx->connect_array[i].id, ctx->connect_array[i].inqueue->queue[j], 40960);
        }
    }
}

void free_rdma_resources(struct jia_context *ctx) {
    for (int i = 0; i < QueueSize; i++) {
        ibv_dereg_mr(ctx->out_mr[i]);
    }

    for (int i = 0; i < system_setting.hostc; i++) {
        if (i == system_setting.jia_pid)
            continue;
        for (int j = 0; j < QueueSize; j++) {
            ibv_dereg_mr(ctx->connect_array[i].in_mr[j]);
        }
    }

    free_msg_queue(ctx->outqueue);
    for (int i = 0; i < system_setting.hostc; i++) {
        free_msg_queue(ctx->connect_array[i].inqueue);
    }

    if (ibv_dealloc_pd(ctx->pd)) {
        log_err("Couldn't dealloc pd");
    }

    if (ibv_close_device(ctx->context)) {
        log_err("Couldn't close device");
    }
}