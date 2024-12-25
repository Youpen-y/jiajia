#include "comm.h"
#include "rdma_comm.h"
#include <pthread.h>
#include <semaphore.h>

extern pthread_cond_t cond;
pthread_t rdma_server_tid;
jia_msg_t msg;

void *rdma_server(void *arg) {
    int value = 0;
    while (1) {
        sem_wait(&ctx.inqueue->busy_count);

        if (ctx.inqueue->queue[ctx.inqueue->head].state == SLOT_BUSY) {
            dequeue(ctx.inqueue, &msg);
            msg_handle(&msg);
        }
        ctx.inqueue->head = (ctx.inqueue->head + 1) % ctx.inqueue->size;
        sem_post(&ctx.inqueue->free_count);
        sem_getvalue(&ctx.inqueue->free_count, &value);
        if (value == ctx.batching_num) {
            pthread_cond_signal(&cond);
        }
    }
}