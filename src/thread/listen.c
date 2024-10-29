#include "thread.h"
#include <sys/epoll.h>

pthread_t listen_tid;
void *listen_thread(void *args)
{


}

void addfd(int epollfd, int fd, bool one_shot, int trigger_mode) {
    epoll_event event;
    event.data.fd = fd;

    if (1 == trigger_mode) {
        event.events = EPOLLIN | EPOLLET;
    } else {
        event.events = EPOLLIN;
    }

    if (one_shot) {
        event.events |= EPOLLONESHOT;  // 单次触发，收到此事件后需重新添加
    }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
}