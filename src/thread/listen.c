#include "thread.h"
#include "comm.h"
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
        perror("epoll_create");
        exit(1);
    }

    // add rcv_fds to epollfd instance
    for (int i = 0; i < Maxhosts; i++)
    {
        addfd(epollfd, comm_manager.rcv_fds[i], false, 0);
    }

    epoll_event events[Maxhosts];

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
                if (ret == -1)
                {
                    perror("recvfrom");
                    continue;
                }

                unsigned seqno = msg.seqno + 1;

                // construct an ack msg
                ackt_t ack;
                ack.seqno = seqno;
                ack.sid = msg.from_id;

                // return an ack
                int to_id = msg.from_id;
                ack_addr.sin_family = AF_INET;
                ack_addr.sin_port = htons(comm_manager.ack_port);
                ack_addr.sin_addr.s_addr = inet_addr(system_setting.hosts[to_id].ip);

                ret = sendto(comm_manager.snd_fds[1], &ack, sizeof(ack), 0, (struct sockaddr *)&ack_addr, sizeof(ack_addr));
                local_assert((res != -1), "ack sendto failed");

                if (seqno == comm_manager.rcv_seq[to_id] + 1)
                { // new msg
                    comm_manager.rcv_seq[to_id] = seqno;
                    enqueue(&inqueue, &msg);
                }
                else
                {
                    // drop the msg, don't do anything
                    VERBOSE_LOG(3, "Receive resend msg");
                }
            }
        }
    }
}

void addfd(int epollfd, int fd, bool one_shot, int trigger_mode)
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

    if (one_shot)
    {
        event.events |= EPOLLONESHOT; // 单次触发，收到此事件后需重新添加
    }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
}