#include "thread.h"
#include "global.h"
#include "comm.h"
#include "utils.h"
#include "mem.h"
#include "syn.h"
#include "load.h"
#include "tools.h"
#include "global.h"
#include "stat.h"

pthread_t server_tid;
void *server_thread(void *args)
{
    msg_queue_t *inqueue = (msg_queue_t *)args;
    jia_msg_t msg;

    while (1) {
        if (dequeue(inqueue, &msg) == -1) {
            perror("msg_queue dequeue");
            continue;
        } else {
            // there, should have a condition (msg.seqno == comm_manager.rcv_seq[msg.frompid]
            msg_handle(&msg);
        }
    }
}

void msg_handle(jia_msg_t *msg) {
    VERBOSE_LOG(3, "In servermsg!\n");
    SPACE(1);

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
            printmsg(msg, 1);
            local_assert(0, "msgserver(): Incorrect Message!");
        }
        break;
    }
    SPACE(1);
    VERBOSE_LOG(3, "Out servermsg!\n");
}
