#include "thread.h"
#include "comm.h"

void *client_thread(void *args) {
    while (1) {
        outsend();
    }
}

// int move_msg_to_outqueue(msg_buffer_t *msg_buffer, msg_queue_t *outqueue)
// {
//     sem_wait(&msg_buffer->busy_count);
//     sem_wait(&outqueue->free_count);
//     for(int i = 0; i < msg_buffer->size; i++) {
//         buffer_slot_t *slot = &msg_buffer->buffer[i];
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

int outsend()
{}

pthread_t client_tid;