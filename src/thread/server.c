#include "msg.h"
#include "thread.h"
#include "global.h"
#include "comm.h"
#include "mem.h"
#include "syn.h"
#include "load.h"
#include "tools.h"
#include "global.h"
#include "stat.h"

static jia_msg_t msg;
static void msg_handle(jia_msg_t *msg);

void *server_thread(void *arg)
{
    struct inqueue_arg *thread_arg = (struct inqueue_arg *)arg;

    msg_queue_t *inqueue = (msg_queue_t *)thread_arg->addr;
    int from_pid = thread_arg->id;

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

/**
 * @brief msg_handle - handle msg
 * 
 * @param msg 
 * @note msg_handle called by server_thread
 */
static void msg_handle(jia_msg_t *msg) {
    log_info(3, "In servermsg!");

    switch (msg->op) {
    case DIFF:
        diffserver(msg);
        break;
    case DIFFGRANT:
        diffgrantserver(msg);
        break;
    case GETP:
        getpserver(msg);
        break;
    case GETPGRANT:
        getpgrantserver(msg);
        break;
    case ACQ:
        acqserver(msg);
        break;
    case ACQGRANT:
        acqgrantserver(msg);
        break;
    case INVLD:
        invserver(msg);
        break;
    case BARR:
        barrserver(msg);
        break;
    case BARRGRANT:
        barrgrantserver(msg);
        break;
    case REL:
        relserver(msg);
        break;
    case WTNT:
        wtntserver(msg);
        break;
    case JIAEXIT:
        jiaexitserver(msg);
        break;
    case WAIT:
        waitserver(msg);
        break;
    case WAITGRANT:
        waitgrantserver(msg);
        break;
    case STAT:
        statserver(msg);
        break;
    case STATGRANT:
        statgrantserver(msg);
        break;
    case SETCV:
        setcvserver(msg);
        break;
    case RESETCV:
        resetcvserver(msg);
        break;
    case WAITCV:
        waitcvserver(msg);
        break;
    case CVGRANT:
        cvgrantserver(msg);
        break;
    case MSGBODY:
    case MSGTAIL:
        msgrecvserver(msg);
        break;
    case LOADREQ:
        loadserver(msg);
        break;
    case LOADGRANT:
        loadgrantserver(msg);
        break;

    default:
        if (msg->op >= BCAST) {
            bcastserver(msg);
        } else {
            printmsg(msg);
            local_assert(0, "msgserver(): Incorrect Message!");
        }
        break;
    }
    log_info(3, "Out servermsg!\n");
}
