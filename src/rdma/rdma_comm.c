#include "rdma_comm.h"
#include "global.h"
#include "setting.h"
#include "tools.h"
#include <infiniband/verbs.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <time.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <unistd.h>

#define Msgsize 200     // temp value, used inlined message size in rdma

extern long start_port;
int batching_num = 8;

jia_context_t ctx;
jia_dest_t dest_info[Maxhosts];
int arg[Maxhosts];

void *tcp_server_thread(void *arg) {
    int server_id = *(int *)arg;
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        log_err("Failed to create server socket");
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)) < 0) {
        perror("setsockopt SO_REUSEADDR/SO_REUSEPORT failed");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr((char *)system_setting.hosts[system_setting.jia_pid].ip);
    server_addr.sin_port = htons(ctx.tcp_port + server_id);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 1) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on <%s, %d>...\n", system_setting.hosts[system_setting.jia_pid].ip, ctx.tcp_port + server_id);

    struct sockaddr_in client_addr = {0};
    socklen_t addrlen = sizeof(client_addr);
    int new_socket = accept(server_fd, (struct sockaddr *)&client_addr, &addrlen);
    if (new_socket < 0) {
        perror("accept");
        exit(EXIT_FAILURE);
    }

    printf("Server: connection established with client.\n");

    bool flag = true;
    while (1) {
        ssize_t bytes_read = recv(new_socket, &dest_info[server_id], sizeof(jia_dest_t), 0);
        if (bytes_read == sizeof(jia_dest_t)) {
            send(new_socket, &flag, sizeof(bool), 0);
            break;
        } else {
            flag = false;
            send(new_socket, &flag, sizeof(bool), 0);
        }
    }

    printf("[%d]\t0x%04x\t0x%06x\t0x%06x\t%016llx:%016llx\n", server_id, dest_info[server_id].lid, dest_info[server_id].qpn, dest_info[server_id].psn, dest_info[server_id].gid.global.subnet_prefix, dest_info[server_id].gid.global.interface_id);

    close(new_socket);
    close(server_fd);
    return NULL;
}

void *tcp_client_thread(void *arg) {
    int client_id = *(int *)arg;
    int client_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client_fd < 0) {
        log_err("Failed to create client socket");
    }

    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(system_setting.hosts[client_id].ip);
    server_addr.sin_port = htons(ctx.tcp_port + system_setting.jia_pid);

    struct timeval timeout;
    timeout.tv_sec = 2;  // 超时秒数
    timeout.tv_usec = 0; // 微秒部分

    if (setsockopt(client_fd, SOL_SOCKET, SO_SNDTIMEO, (const char *)&timeout, sizeof(timeout)) < 0) {
        perror("setsockopt failed");
        close(client_fd);
    }

    while(connect(client_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        if (errno == EINPROGRESS || errno == EWOULDBLOCK) {
            printf("Connection timed out\n");
        }
    }

    bool flag = false;
    while (1) {
        send(client_fd, &dest_info[system_setting.jia_pid], sizeof(jia_dest_t), 0);
        if (recv(client_fd, &flag, sizeof(bool), 0) == 1 && flag) {
            printf("server returned %s\n", flag==true?"true":"false");
            break;
        }
    }

    close(client_fd);
    return NULL;
}

void exchange_dest_info() {
    for (int i = 0; i < Maxhosts; i++) {
        arg[i] = i;
    }

    for (int i = 0; i < system_setting.hostc; i++) {
        if (i == system_setting.jia_pid) {
            continue;
        }
        pthread_create(&ctx.server_tid[i], NULL, tcp_server_thread, &arg[i]);
    }

    for (int i = 0; i < system_setting.hostc; i++) {
        if (i == system_setting.jia_pid) {
            continue;
        }
        pthread_create(&ctx.client_tid[i], NULL, tcp_client_thread, &arg[i]);
    }

    for (int i = 0; i < system_setting.hostc; i++) {
        if (i == system_setting.jia_pid) {
            continue;
        }
        pthread_join(ctx.server_tid[i], NULL);
        pthread_join(ctx.client_tid[i], NULL);
    }

    printf("jia_pid\tlid\tqpn\tpsn\tgid.subnet_prefix:gid.interface_id\n");
    for(int i = 0; i < system_setting.hostc; i++) {
        printf("[%d]\t0x%04x\t0x%06x\t0x%06x\t%016llx:%016llx\n", i, dest_info[i].lid, dest_info[i].qpn, dest_info[i].psn, dest_info[i].gid.global.subnet_prefix, dest_info[i].gid.global.interface_id);
    }
}

int init_rdma_context(struct jia_context *ctx, int batching_num) {
    struct ibv_device **dev_list;
    struct ibv_device *dev;

    dev_list = ibv_get_device_list(NULL);
    if (!dev_list) {
        log_info(3, "No RDMA device available");
        return -1;
    }

    dev = dev_list[0];  // use the first device
    ctx->context = ibv_open_device(dev);
    if (!dev) {
        log_info(3, "Open first device failed");
    }

    ctx->send_channel = ibv_create_comp_channel(ctx->context);
    ctx->recv_channel = ibv_create_comp_channel(ctx->context);

    ctx->pd = ibv_alloc_pd(ctx->context);
    
    ctx->send_mr = (struct ibv_mr **)malloc(sizeof(struct ibv_mr * ) * system_setting.msg_queue_size);
    ctx->recv_mr = (struct ibv_mr **)malloc(sizeof(struct ibv_mr *) * system_setting.msg_queue_size);

    ctx->outqueue = &outqueue;
    ctx->inqueue = &inqueue;

    for (int i = 0; i < system_setting.msg_queue_size; i++) {
        ctx->send_mr[i] = ibv_reg_mr(ctx->pd, &ctx->outqueue->queue[i].msg, sizeof(jia_msg_t), IBV_ACCESS_LOCAL_WRITE);
        ctx->recv_mr[i] = ibv_reg_mr(ctx->pd, &ctx->inqueue->queue[i].msg, sizeof(jia_msg_t), IBV_ACCESS_LOCAL_WRITE);
    }

    ctx->send_cq = ibv_create_cq(ctx->context, system_setting.msg_queue_size, NULL, ctx->send_channel, 0);
    ctx->recv_cq = ibv_create_cq(ctx->context, system_setting.msg_queue_size, NULL, ctx->recv_channel, 0);

    ctx->ib_port = 2;   // this can be set from a config file

    ctx->batching_num = batching_num;
    {
        struct ibv_qp_attr attr;
        struct ibv_qp_init_attr init_attr = {
            .qp_context = ctx->context,
            .send_cq = ctx->send_cq,
            .recv_cq = ctx->recv_cq,
            .srq = NULL,
            .cap = {
                .max_send_wr = system_setting.msg_queue_size,
                .max_recv_wr = system_setting.msg_queue_size,
                .max_send_sge = 1,
                .max_recv_sge = 1,
               .max_inline_data = 0,
            },
            .qp_type = IBV_QPT_UD,
            .sq_sig_all = 1
        };

        ctx->qp = ibv_create_qp(ctx->pd, &init_attr);

        ibv_query_qp(ctx->qp, &attr, IBV_QP_CAP, &init_attr);
		if (init_attr.cap.max_inline_data >= Msgsize) {
            // use inline data for little messages
			ctx->send_flags |= IBV_SEND_INLINE;
		}

        attr.qp_state = IBV_QPS_INIT;
        attr.pkey_index = 0;
        attr.port_num = ctx->ib_port;
        attr.qkey = 0x11111111;

        if (ibv_modify_qp(ctx->qp, &attr,
                IBV_QP_STATE              |
                IBV_QP_PKEY_INDEX         |
                IBV_QP_PORT               |
                IBV_QP_QKEY)) {
            log_err("Failed to modify QP to INIT");
        }

        attr.qp_state = IBV_QPS_RTR;
        if (ibv_modify_qp(ctx->qp, &attr,
                IBV_QP_STATE              |
                IBV_QP_QKEY)) {
            log_err("Failed to modify QP to RTR");
        }

        attr.qp_state = IBV_QPS_RTS;
        attr.sq_psn = 0;
        if (ibv_modify_qp(ctx->qp, &attr,
                IBV_QP_STATE              |
                IBV_QP_SQ_PSN)) {
            log_err("Failed to modify QP to RTS");
        }
    }

    ibv_query_port(ctx->context, ctx->ib_port, &ctx->portinfo);

    dest_info[system_setting.jia_pid].lid = ctx->portinfo.lid;
    dest_info[system_setting.jia_pid].qpn = ctx->qp->qp_num;
    dest_info[system_setting.jia_pid].psn = 0;
    
    if (ibv_query_gid(ctx->context, ctx->ib_port, 0, &dest_info[system_setting.jia_pid].gid)) {
        log_err("Failed to query gid");
        memset(&dest_info[system_setting.jia_pid].gid, 0, sizeof(dest_info[system_setting.jia_pid].gid));
    }

    log_info(3, "Local address: LID 0x%04x, QPN 0x%06x, PSN 0x%06x, GID %016llx:%016llx", dest_info[system_setting.jia_pid].lid, dest_info[system_setting.jia_pid].qpn, dest_info[system_setting.jia_pid].psn, dest_info[system_setting.jia_pid].gid.global.subnet_prefix, dest_info[system_setting.jia_pid].gid.global.interface_id);

    ctx->tcp_port = start_port;
    exchange_dest_info();

    // create ah
    ctx->ah = (struct ibv_ah **)malloc(sizeof(struct ibv_ah *) * system_setting.hostc);
    for (int i = 0; i < system_setting.hostc; i++) {
        if (i == system_setting.jia_pid) {
            continue;
        }
        struct ibv_ah_attr attr = {
            .is_global = 0,
            .dlid = dest_info[i].lid,
            .sl = 0,
            .src_path_bits = 0,
            .port_num = ctx->ib_port
        };

        if (dest_info[i].gid.global.interface_id) {
            attr.is_global = 1;
            attr.grh.hop_limit = 1;
            attr.grh.dgid = dest_info[i].gid;
            attr.grh.sgid_index = 0;
        }

        ctx->ah[i] = ibv_create_ah(ctx->pd, &attr);
    }
    return 0;
}

void init_rdma_comm() {
    if (system_setting.jia_pid == 0) {
        VERBOSE_LOG(3, "************Initialize RDMA Communication!*******\n");
    }
    VERBOSE_LOG(3, "current jia_pid = %d\n", system_setting.jia_pid);
    VERBOSE_LOG(3, " start_port = %ld \n", start_port);
    
    /* step 1: init msg buffer */
    init_msg_buffer(&msg_buffer, system_setting.msg_buffer_size);

    /* step 2: init inqueue, outqueue msg queue */
    init_msg_queue(&inqueue, system_setting.msg_queue_size);
    init_msg_queue(&outqueue, system_setting.msg_queue_size);

    /* step 3: init rdma context */
    init_rdma_context(&ctx, batching_num);

    /* step 4: create rdma client, server, listen thread */
    pthread_create(&rdma_client_tid, NULL, rdma_client, NULL);
    pthread_create(&rdma_server_tid, NULL, rdma_server, NULL);
    pthread_create(&rdma_listen_tid, NULL, rdma_listen, NULL);

    /* step 5: register signal handler */
    register_sigint_handler();
}


void free_rdma_resources(struct jia_context *ctx){
    if (ibv_destroy_qp(ctx->qp)) {
        log_err("Couldn't destroy QP\n");
    }

    if (ibv_destroy_cq(ctx->send_cq)) {
        log_err("Couldn't destroy Send CQ\n");
    }

    if (ibv_destroy_cq(ctx->recv_cq)) {
        log_err("Couldn't destroy Recv CQ\n");
    }

    for (int i = 0; i < system_setting.msg_queue_size; i++) {
        if (ibv_dereg_mr(ctx->send_mr[i])) {
            log_err("Couldn't dereg send mr[%d]", i);
        }
        if (ibv_dereg_mr(ctx->recv_mr[i])) {
            log_err("Couldn't dereg recv mr[%d]", i);
        }
    }

    for (int i = 0; i < system_setting.hostc; i++) {
        if (ibv_destroy_ah(ctx->ah[i])) {
            log_err("Couldn't destroy ah[%d]", i);
        }
    }
    free(ctx->ah);

    if (ibv_dealloc_pd(ctx->pd)) {
        log_err("Couldn't deallocate PD");
    }

    if (ibv_destroy_comp_channel(ctx->send_channel)) {
        log_err("Couldn't destroy send completion channel");
    }

    if (ibv_destroy_comp_channel(ctx->recv_channel)) {
        log_err("Couldn't destroy recv completion channel");
    }

    if (ibv_close_device(ctx->context)) {
        log_err("Couldn't release context");
    }
}