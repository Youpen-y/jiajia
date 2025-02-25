#include "comm.h"
#include "global.h"
#include "tools.h"
#include "udp.h"

pthread_t server_tid;

static jia_msg_t* msg_ptr;

void *server_thread(void *args) {
    while (1) {
        /* step 0: post sem value && get msg_ptr */
        int semvalue;
        if (sem_wait(&comm_manager.inqueue->busy_count) != 0) {
            log_err("sem wait fault");
        }
        sem_getvalue(&comm_manager.inqueue->busy_count, &semvalue);
        log_info(4, "enter server inqueue dequeue! busy_count value: %d", semvalue);
        msg_ptr = dequeue(comm_manager.inqueue);

        /* step 1: print info && handle msg */
        // there, should have a condition (msg.seqno == comm_manager.rcv_seq[msg.frompid])
        log_info(3, "dequeue msg<seqno:%d, op:%s, frompid:%d, topid:%d>", msg_ptr->seqno,
                 op2name(msg_ptr->op), msg_ptr->frompid, msg_ptr->topid);

        msg_handle(msg_ptr);

        /* step 2: sem post and print value */
        sem_post(&comm_manager.inqueue->free_count);
        sem_getvalue(&comm_manager.inqueue->free_count, &semvalue);
        log_info(4, "after server inqueue dequeue free_count value: %d", semvalue);
    }
}
