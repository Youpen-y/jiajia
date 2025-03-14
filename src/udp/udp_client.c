#include "comm.h"
#include "msg.h"
#include "setting.h"
#include "stat.h"
#include "tools.h"
#include "udp.h"
#include <errno.h>
#include <stdbool.h>
#include <sys/epoll.h>
#include <sys/select.h>
#include <unistd.h>

#define RETRYNUM 50 // when hosts increases, this number should increases too.
pthread_t client_tid;

static jia_msg_t* msg_ptr;
static int epollfd;
static int outsend(jia_msg_t *msg);
static void addfd(int epollfd, int fd, int trigger_mode);

void *client_thread(void *args) {
    /* step 1: create epollfd */
    epollfd = epoll_create(1);
    if (epollfd == -1) {
        log_err("epoll_create failed");
        exit(1);
    }
    addfd(epollfd, comm_manager.ack_fds, 1);

    /* step 2: dequeue msg to send */
    bool success = false;
    while (1) {
        /* step 0: post sem value && get msg_ptr */
        int semvalue;
        if (sem_wait(&comm_manager.outqueue->busy_count) != 0) {
            log_err("sem wait fault");
        }
        sem_getvalue(&comm_manager.outqueue->busy_count, &semvalue);
        log_info(4, "enter client outqueue dequeue! busy_count value: %d", semvalue);
        msg_ptr = dequeue(comm_manager.outqueue);

        /* step 1: give seqno */
        msg_ptr->seqno = comm_manager.snd_seq[msg_ptr->topid];

        /* step 2: send msg && ack */
        for (int retries_num = 0; retries_num < RETRYNUM; retries_num++) {
            if (!outsend(msg_ptr)) {
                success = true;
                break;
            }
#ifdef DOSTAT
            STATOP(jiastat.resendcnt++;)
#endif
        }

        /* step 3: manage error */
        if (success) {
            log_info(4, "send msg success!");
            success = false;
        } else {
            log_err("send msg failed[msg: %lx]", (unsigned long)msg_ptr);
            printmsg(msg_ptr);
        }

        /* step 4: update snd_seq */
        comm_manager.snd_seq[msg_ptr->topid]++;

        /* step 5: sem post and print value */
        sem_post(&(comm_manager.outqueue->free_count));
        sem_getvalue(&comm_manager.outqueue->free_count, &semvalue);
        log_info(4, "after client outqueue dequeue free_count value: %d", semvalue);
    }
}

/**
 * @brief outsend - send msg to destination host
 *
 * @param msg msg to send
 * @return int
 */
static int outsend(jia_msg_t *msg) {
    int state;
    struct epoll_event event;

    if (msg == NULL) {
        perror("msg is NULL");
        return -1;
    }

#ifdef DOSTAT
    STATOP(if (msg->size > 4096) jiastat.largecnt++; if (msg->size < 128) jiastat.smallcnt++;)
#endif

    int ret;
    struct sockaddr_in to_addr;
    ack_t ack = {-1, -1};

    if (msg->topid == msg->frompid) {
        log_info(3, "msg<seqno:%d, op:%s, frompid:%d, topid:%d> is send to self", msg->seqno,
                 op2name(msg->op), msg->frompid, msg->topid);
        return enqueue(&inqueue, msg);
    } else {

#ifdef DOSTAT
        if (statflag == 1) {
            jiastat.msgsndcnt++;
            jiastat.msgsndbytes +=
                (((jia_msg_t *)comm_manager.outqueue->queue[comm_manager.outqueue->head])->size +
                 Msgheadsize);
        }
#endif

        /* step 1: send msg to destination host with ip */
        to_addr.sin_family = AF_INET;
        to_addr.sin_port = htons(comm_manager.snd_server_port);
        to_addr.sin_addr.s_addr = inet_addr(system_setting.hosts[msg->topid].ip);
        ret = sendto(comm_manager.snd_fds, msg, sizeof(jia_msg_t), 0, (struct sockaddr *)&to_addr,
                     sizeof(struct sockaddr));
        if (ret == sizeof(jia_msg_t)) {
            log_info(3, "msg <seqno:%d, op:%s, frompid:%d, topid:%d> is send to %s", msg->seqno,
                     op2name(msg->op), msg->frompid, msg->topid,
                     system_setting.hosts[msg->topid].ip)
        }

        while (1) {
            int nfds = epoll_wait(epollfd, &event, 1, TIMEOUT);
            if (nfds <= 0) {
                log_info(3, "TIMEOUT! try resend");
                state = -1;
                break;
            }

            if (event.data.fd == comm_manager.ack_fds && event.events & EPOLLIN) {
                ret = recvfrom(comm_manager.ack_fds, &ack, sizeof(ack_t), 0, NULL, NULL);
                if (ret == sizeof(ack_t)) {
                    if (ack.sid == msg->topid && ack.seqno == (msg->seqno + 1)) {
                        log_info(3, "get ack<seqno:%d, sid:%d> success", ack.seqno, ack.sid);
                        state = 0;
                        break;
                    } else {
                        log_err("Not match[<ack.seqno: %u ack.sid: %u> xxx <msg.seqno: %u "
                                "msg.topid: %u], ack will be ignored",
                                ack.seqno, ack.sid, msg->seqno, msg->topid);
                    }
                } else {
                    log_err("ack size error! try resend");
                    state = -1;
                    break;
                }
            }
        }
        return state;
    }

    return -1;
}

/**
 * @brief addfd - add fd to epollfd instance

 *
 * @param epollfd epollfd instance
 * @param fd fd to add
 * @param trigger_mode trigger mode, 1 for edge trigger, 0 for level trigger
 */
static void addfd(int epollfd, int fd, int trigger_mode) {
    struct epoll_event event;
    event.data.fd = fd;

    if (1 == trigger_mode) {
        event.events = EPOLLIN | EPOLLET;
    } else {
        event.events = EPOLLIN;
    }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
}
