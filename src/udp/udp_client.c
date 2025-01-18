#include "comm.h"
#include "msg.h"
#include "setting.h"
#include "stat.h"
#include "udp.h"
#include "tools.h"
#include <errno.h>
#include <stdbool.h>
#include <sys/epoll.h>
#include <sys/select.h>
#include <unistd.h>

#define RETRYNUM 50 // when hosts increases, this number should increases too.
static bool success = false;
static jia_msg_t msg;
pthread_t client_tid;
static fd_set readfd;
static int outsend(jia_msg_t *msg);
static struct timeval timeout = {TIMEOUT, 0};
static struct epoll_event event;
static int epollfd;
static void addfd(int epollfd, int fd, int trigger_mode);

void *client_thread(void *args) {
    msg_queue_t *outqueue = (msg_queue_t *)args;
    /* create epollfd */
    epollfd = epoll_create(1);
    if (epollfd == -1) {
        log_err("epoll_create failed");
        exit(1);
    }
    addfd(epollfd, comm_manager.ack_fds, 1);

    while (1) {
        if (dequeue(outqueue, &msg)) {
            log_info(3, "outqueue is null");
            continue;
        } else {
            /* step 1: give seqno */
            msg.seqno = comm_manager.snd_seq[msg.topid];

            /* step 2: send msg && ack */
            for (int retries_num = 0; retries_num < RETRYNUM; retries_num++) {
                if (!outsend(&msg)) {
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
                log_err("send msg failed[msg: %lx]", (unsigned long)&msg);
                printmsg(&msg);
            }

            /* step 4: update snd_seq */
            comm_manager.snd_seq[msg.topid]++;
        }
    }
}

/**
 * @brief outsend - send msg to destination host
 *
 * @param msg msg to send
 * @return int
 */
static int outsend(jia_msg_t *msg) {
    if (msg == NULL) {
        perror("msg is NULL");
        return -1;
    }

#ifdef DOSTAT
    STATOP(if (msg->size > 4096) jiastat.largecnt++;
           if (msg->size < 128) jiastat.smallcnt++;)
#endif

    int ret;
    struct sockaddr_in to_addr;
    ack_t ack = {-1, -1};

    if (msg->topid == msg->frompid) {
        log_info(3, "msg<seqno:%d, op:%s, frompid:%d, topid:%d> is send to self",
                msg->seqno, op2name(msg->op), msg->frompid, msg->topid);
        return enqueue(&inqueue, msg);
    } else {

#ifdef DOSTAT
        if (statflag == 1) {
            jiastat.msgsndcnt++;
            jiastat.msgsndbytes +=
                (((jia_msg_t *)outqueue.queue[outqueue.head])->size + Msgheadsize);
        }
#endif

        /* step 1: send msg to destination host with ip */
        to_addr.sin_family = AF_INET;
        to_addr.sin_port = htons(comm_manager.snd_server_port);
        to_addr.sin_addr.s_addr =
            inet_addr(system_setting.hosts[msg->topid].ip);
            ret = sendto(comm_manager.snd_fds, msg, sizeof(jia_msg_t), 0,
                        (struct sockaddr *)&to_addr, sizeof(struct sockaddr));
                if (ret == sizeof(jia_msg_t)) {
            log_info(
                3, "msg <seqno:%d, op:%s, frompid:%d, topid:%d> is send to %s",
                msg->seqno, op2name(msg->op), msg->frompid, msg->topid,
                system_setting.hosts[msg->topid].ip)
        }

        int nfds = epoll_wait(epollfd, &event, 1, TIMEOUT);
        if (nfds <= 0){
            log_info(3, "TIMEOUT! try resend");
            return -1;
        }

        if (event.data.fd == comm_manager.ack_fds && event.events & EPOLLIN) {
            ret = recvfrom(comm_manager.ack_fds, &ack, sizeof(ack_t), 0, NULL,
                           NULL);
            if (ret == sizeof(ack_t) && (ack.seqno == (msg->seqno + 1))) {
                log_info(3, "get ack<seqno:%d, sid:%d> success", ack.seqno,
                         ack.sid);
                return 0;
            }
            if (ret == -1) {
                log_err("ack error! try resend");
                return -1;
            }
            // this cond may not happen
            if (ack.seqno != (msg->seqno + 1)) {
                log_err("seqno not match[ack.seqno: %u msg.seqno: %u]",
                        ack.seqno, msg->seqno);
                return -1;
            }
        }
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
