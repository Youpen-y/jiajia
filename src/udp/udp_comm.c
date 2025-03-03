#include "comm.h"
#include "setting.h"
#ifndef NULL_LIB
#include "udp.h"
#include "stat.h"
#include "tools.h"
#include <pthread.h>
#include <semaphore.h>
#include <stdatomic.h>
#include <sys/socket.h>

/* communication manager */
comm_manager_t comm_manager;

/**
 * @brief fd_create -- creat socket file descriptor used to send and recv
 * request
 *
 * @param i the index of host
 * @param flag fd create flag, FDCR_ACK for ack port / FDCR_RECV for recv port /
 * FDCR_SEND for send port
 *
 * @return int socket file descriptor
 * creat socket file descriptor(fd) used to send and recv request and bind it to
 * an address (ip/port combination)
 */
static int fd_create(int port) {
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
    addr.sin_port = htons(port); 

    res = bind(fd, (struct sockaddr *)&addr, sizeof(addr));
    local_assert((res == 0), "req_fdcreate()-->bind()");

    return fd;
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
        comm_manager.rcv_fds[i] = fd_create(comm_manager.rcv_ports[i]);
        set_nonblocking(comm_manager.rcv_fds[i]);
    }
    // snd_fds socket fd with random port
    comm_manager.snd_fds = fd_create(0);
    // ack_fds socket fd with ack port
    comm_manager.ack_fds = fd_create(comm_manager.ack_port);
    set_nonblocking(comm_manager.ack_fds);

    /** step 3:init seq */
    for (int i = 0; i < Maxhosts; i++) {
        comm_manager.snd_seq[i] = 0;
        comm_manager.ack_seq[i] = 0;
        comm_manager.rcv_seq[i] = 0;
    }

    return 0;
}

/**
 * @brief initcomm -- initialize communication setting
 */
void init_udp_comm() {
#ifdef DOSTAT
    register unsigned int begin = get_usecs();
#endif

    int i, j, fd;

    if (system_setting.jia_pid == 0) {
        VERBOSE_LOG(3, "************Initialize Communication!*******\n");
    }
    VERBOSE_LOG(3, "current jia_pid = %d\n", system_setting.jia_pid);
    VERBOSE_LOG(3, " start_port = %ld \n", start_port);

    /* step 1: init msg buffer */
    init_msg_buffer(&msg_buffer, system_setting.msg_buffer_size);

    /* step 2: init inqueue, outqueue msg queue */
    comm_manager.inqueue = &inqueue;
    comm_manager.outqueue = &outqueue;
    init_msg_queue(comm_manager.inqueue, system_setting.msg_queue_size);
    init_msg_queue(comm_manager.outqueue, system_setting.msg_queue_size);

    /* step 3: init comm manager */
    init_comm_manager();

    /* step 4: create client, server, listen threads */
    pthread_create(&client_tid, NULL, client_thread,
                   &outqueue); // create a new thread to send msg from outqueue
    pthread_create(&server_tid, NULL, server_thread,
                   &inqueue); // create a new thread to serve msg from inqueue
    pthread_create(&listen_tid, NULL, listen_thread, NULL);

    /* step 5: register sigint handler */
    register_sigint_handler();

#ifdef DOSTAT
    jiastat.initcomm += get_usecs() - begin;
#endif
}

#else  /* NULL_LIB */
#endif /* NULL_LIB */