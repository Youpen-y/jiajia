#include "msg.h"
#include "thread.h"
#include "comm.h"
#include "setting.h"
#include "tools.h"
#include "utils.h"
#include <stdlib.h>
#include <sys/epoll.h>
#include <stdbool.h>

pthread_t listen_tid;
void *listen_thread(void *args)
{
    struct sockaddr_in ack_addr;

    // create epollfd epoll instance
    int epollfd = epoll_create(1);
    if (epollfd == -1)
    {
        log_err("epoll_create failed");
        exit(1);
    }

    // add rcv_fds to epollfd instance
    for (int i = 0; i < Maxhosts; i++)
    {
        addfd(epollfd, comm_manager.rcv_fds[i], 1);
    }

    struct epoll_event events[Maxhosts];

    while (1)
    {
        // timeout = -1, block forever until an event occurs
        int nfds = epoll_wait(epollfd, events, Maxhosts, -1);
        if (nfds == -1)
        {
            perror("epoll_wait");
            exit(1);
        }

        for (int i = 0; i < nfds; i++)
        {
            int sockfd = events[i].data.fd;
            if (events[i].events & EPOLLIN)
            {
                jia_msg_t msg;

                // receive a msg
                int ret = recvfrom(sockfd, &msg, sizeof(jia_msg_t), 0, NULL, NULL);
                if (ret != sizeof(jia_msg_t))
                {
                    log_err("recvfrom failed, only got %d bytes", ret);
                    continue;
                }

                unsigned seqno = msg.seqno + 1;

                // construct an ack msg
                ack_t ack;
                ack.seqno = seqno;
                ack.sid = msg.frompid;

                // return an ack
                int to_id = msg.frompid;
                ack_addr.sin_family = AF_INET;
                ack_addr.sin_port = htons(comm_manager.ack_port);
                ack_addr.sin_addr.s_addr = inet_addr(system_setting.hosts[to_id].ip);

                ret = sendto(comm_manager.snd_fds, &ack, sizeof(ack), 0, (struct sockaddr *)&ack_addr, sizeof(ack_addr));
                local_assert((ret != -1), "ack sendto failed");

                if (seqno == comm_manager.rcv_seq[to_id] + 1)
                { // new msg
                    comm_manager.rcv_seq[to_id] = seqno;
                    enqueue(&inqueue, &msg);
                }
                else
                {
                    // drop the msg, don't do anything
                    log_info(3, "Receive resend msg");
                }
            }
        }
    }
}

void addfd(int epollfd, int fd, int trigger_mode)
{
    struct epoll_event event;
    event.data.fd = fd;

    if (1 == trigger_mode)
    {
        event.events = EPOLLIN | EPOLLET;
    }
    else
    {
        event.events = EPOLLIN;
    }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
}