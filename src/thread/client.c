#include "thread.h"

void *client_thread(void *args) {
    int i;
    jia_msg_t *msg;
    int fd;

    while (1) {
        msg = dequeue(&inqueue);
        STATOP(printf("client: dequeue msg, msg->type = %d\n", msg->type));
        fd = req_fdcreate(msg->dest, 0);


        

}

pthread_t client_tid;