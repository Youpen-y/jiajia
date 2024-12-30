#include "udp.h"
#include "global.h"
#include "comm.h"
#include "mem.h"
#include "syn.h"
#include "load.h"
#include "tools.h"
#include "global.h"
#include "stat.h"

pthread_t server_tid;
static jia_msg_t msg;
void msg_handle(jia_msg_t *msg);

void *server_thread(void *args)
{
    msg_queue_t *inqueue = (msg_queue_t *)args;

    while (1) {
        if (dequeue(inqueue, &msg) == -1) {
            log_err("msg_queue dequeue");
            continue;
        } else {
            // there, should have a condition (msg.seqno == comm_manager.rcv_seq[msg.frompid])
            log_info(3,"dequeue msg<seqno:%d, op:%s, frompid:%d, topid:%d>", msg.seqno, op2name(msg.op), msg.frompid, msg.topid);

            msg_handle(&msg);
        }
    }
}


