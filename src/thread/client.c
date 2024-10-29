#include "thread.h"
#include "comm.h"

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

int outsend(jia_msg_t *msg)
{
    if (msg == NULL) {
        perror("msg is NULL");
        return -1;
    }

    int to_id, from_id;
    to_id = msg->to_id;
    from_id = msg->from_id;


    int sockfd = comm_manager.snd_fds[to_id];
    struct sockaddr_in *to_addr;
    to_addr.sin_family = AF_INET;
    to_addr.sin_port = htons(comm_manager.snd_ports[to_id]);
    to_addr.sin_addr.s_addr = inet_addr(system_setting.hosts[to_id].ip);
    
    if (to_id == from_id) {
        enqueue(&inqueue, msg);
    } else {
        sendto(sockfd, msg, sizeof(jia_msg_t), 0, (struct sockaddr *)to_addr, sizeof(struct sockaddr));
    }
}
