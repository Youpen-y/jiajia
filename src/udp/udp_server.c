#include "udp.h"
#include "global.h"
#include "comm.h"
#include "tools.h"

pthread_t server_tid;
static jia_msg_t msg;
extern void msg_handle(jia_msg_t *msg);

void *server_thread(void *args)
{
    while (1) {
        if (dequeue(comm_manager.inqueue, &msg) == -1) {
            log_err("msg_queue dequeue");
            continue;
        } else {
            // there, should have a condition (msg.seqno == comm_manager.rcv_seq[msg.frompid])
            log_info(3,"dequeue msg<seqno:%d, op:%s, frompid:%d, topid:%d>", msg.seqno, op2name(msg.op), msg.frompid, msg.topid);

            msg_handle(&msg);
        }
    }
}


