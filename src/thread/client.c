#include "comm.h"
#include "msg.h"
#include "setting.h"
#include "stat.h"
#include "thread.h"
#include "utils.h"
#include <unistd.h>

pthread_t client_tid;
void *client_thread(void *args) {
    msg_queue_t *outqueue = (msg_queue_t *)args;
    jia_msg_t msg;
    while (1) {
        if (dequeue(&outqueue, &msg) == -1) {
            perror("outqueue dequeue");
            continue;
        } else {
            outsend(&msg);
        }
    }
}

void *client_listen(void *args) {
    struct sockaddr_in from;
    int s = sizeof(from);

    while (1) {
        ack_t ack;

        int ret = recvfrom(comm_manager.ack_fds, (char *)&ack, sizeof(ack), 0,
                           (struct sockaddr *)&from, (socklen_t *)&s);
        if (ret == -1) {
            perror("recvfrom");
            continue;
        }


    int seq;
    struct sockaddr_in from;
    int s = sizeof(from);
    comm_manager.ack_seq =
        recvfrom(comm_manager.ack_port, (char *)&seq, Intbytes, 0,
                 (struct sockaddr *)&from, (socklen_t *)&s);
}

// int move_msg_to_outqueue(msg_buffer_t *msg_buffer, msg_queue_t *outqueue)
// {
//     sem_wait(&msg_buffer->busy_count);
//     sem_wait(&outqueue->free_count);
//     for(int i = 0; i < msg_buffer->size; i++) {
//         slot_t *slot = &msg_buffer->buffer[i];
//         pthread_mutex_lock(&slot->lock);
//         if(slot->state == SLOT_BUSY) {
//             ret = enqueue(outqueue, &slot->msg);
//             if(ret == -1) {
//                 perror("enqueue");
//                 continue;
//             }
//             slot->state = SLOT_FREE;
//             pthread_mutex_unlock(&slot->lock);
//             sem_post(&msg_buffer->free_count);
//             sem_post(&outqueue->busy_count);
//         } else {
//             pthread_mutex_unlock(&slot->lock);
//         }
//     }
//     return 0;
// }

int outsend(jia_msg_t *msg) {
    if (msg == NULL) {
        perror("msg is NULL");
        return -1;
    }

    int to_id, from_id;
    to_id = msg->topid;
    from_id = msg->frompid;

    int sockfd = comm_manager.snd_fds[0];
    struct sockaddr_in to_addr;

    if (to_id == from_id) {
        enqueue(&inqueue, msg);
    } else {

#ifdef DOSTAT
        if (statflag == 1) {
            jiastat.msgsndcnt++;
            jiastat.msgsndbytes +=
                (outqueue.queue[outqueue.head].msg.size + Msgheadsize);
        }
#endif

        /* step 1: send msg to destination host with ip */
        to_addr.sin_family = AF_INET;
        to_addr.sin_port = htons(comm_manager.snd_server_port);
        to_addr.sin_addr.s_addr = inet_addr(system_setting.hosts[to_id].ip);
        VERBOSE_LOG(3, "toproc IP address is %u, IP port is %u\n",
                    to_addr.sin_addr.s_addr, to_addr.sin_port);
        sendto(sockfd, msg, sizeof(jia_msg_t), 0, (struct sockaddr *)&to_addr,
               sizeof(struct sockaddr));

        /* step 2: send msg to ip */
    }
}
