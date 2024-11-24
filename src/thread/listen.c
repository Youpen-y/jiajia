#include "comm.h"
#include "msg.h"
#include "setting.h"
#include "stat.h"
#include "thread.h"
#include "tools.h"
#include "utils.h"
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/types.h>

pthread_t listen_tid;
static jia_msg_t msg;
ack_t ack;
struct epoll_event events[Maxhosts];
static void addfd(int epollfd, int fd, int trigger_mode);

void *listen_thread(void *args) {
    struct sockaddr_in ack_addr;

    // create epollfd epoll instance
    int epollfd = epoll_create(Maxhosts);
    if (epollfd == -1) {
        log_err("epoll_create failed");
        exit(1);
    }

    // add rcv_fds to epollfd instance
    for (int i = 0; i < Maxhosts; i++) {
        addfd(epollfd, comm_manager.rcv_fds[i], 1);
    }

    while (1) {
        int nfds = epoll_wait(epollfd, events, Maxhosts, -1);

        for (int i = 0; i < nfds; i++) {
            int sockfd = events[i].data.fd;
            if (events[i].events & EPOLLIN) {
                /* step 1: receive a msg */
                int ret =
                    recvfrom(sockfd, &msg, sizeof(jia_msg_t), 0, NULL, NULL);
                if (ret != sizeof(jia_msg_t)) {
                    log_err("recvfrom failed, only got %d bytes", ret);
                    continue;
                }

                log_info(3,
                        "get msg <seqno:%d, op:%d, "
                        "frompid:%d, topid:%d>",
                        msg.seqno, msg.op, msg.frompid, msg.topid);

                /* step 2: construct an ack msg */
                ack.seqno = msg.seqno + 1;
                ack.sid = msg.topid;

                /* step 3: return an ack */
                int to_id = msg.frompid;
                ack_addr.sin_family = AF_INET;
                ack_addr.sin_port = htons(comm_manager.ack_port);
                ack_addr.sin_addr.s_addr =
                    inet_addr(system_setting.hosts[to_id].ip);

                ret = sendto(comm_manager.snd_fds, &ack, sizeof(ack), 0,
                             (struct sockaddr *)&ack_addr, sizeof(ack_addr));
                if (ret != sizeof(ack_t)) {
                    log_err("sendto ret = %d, ack failed", ret);
                    exit(-1);
                }

                log_info(3, "send ack<seqno:%d, sid:%d> to machine: %s",
                        ack.seqno, ack.sid, system_setting.hosts[to_id].ip);

                /* step 4: enqueue new msg into inqueue */
                if (msg.seqno == comm_manager.rcv_seq[to_id]) {
                comm_manager.rcv_seq[to_id]++;
                log_info(3,
                        "msg<seqno:%d, op:%d, frompid:%d, topid:%d> will "
                        "be enqueued",
                        msg.seqno, msg.op, msg.frompid, msg.topid);
                enqueue(&inqueue, &msg);

#ifdef DOSTAT
                    if (statflag == 1) {
                        jiastat.msgrcvcnt++;
                        jiastat.msgrcvbytes += (msg.size + Msgheadsize);
                    }
#endif

                } else {
                    // drop the msg(msg's seqno is not need), don't do anything
                    log_info(3,
                             "Receive resend msg, msg<seqno:%d, op:%d, "
                             "frompid:%d, topid:%d> will be droped",
                             msg.seqno, msg.op, msg.frompid, msg.topid);
                }
            }
        }
    }
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
