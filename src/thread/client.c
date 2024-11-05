#include "comm.h"
#include "msg.h"
#include "setting.h"
#include "stat.h"
#include "thread.h"
#include "tools.h"
#include "utils.h"
#include <asm-generic/errno.h>
#include <errno.h>
#include <stdbool.h>
#include <sys/select.h>
#include <unistd.h>

#define RETRYNUM 4
static bool success = false;
static jia_msg_t msg;
pthread_t client_tid;
static fd_set fdset;
static int outsend(jia_msg_t *msg);
static struct timeval timeout = {TIMEOUT, 0};

void *client_thread(void *args) {
    msg_queue_t *outqueue = (msg_queue_t *)args;

    while (1) {
        if (dequeue(outqueue, &msg)) {
            log_info(3, "outqueue is null");
            continue;
        } else {
            /* step 1: give seqno */
            msg.seqno = comm_manager.snd_seq[msg.topid];

            /* step 2: send msg && ack */
            for (int retries_num = 0; retries_num < RETRYNUM; retries_num++) {
                if (!outsend(&msg)) {
                    success = true;
                    break;
                }
                
                #ifdef DOSTAT
                    STATOP(jiastat.resendcnt++;)
                #endif
            }

            /* step 3: manage error */
            if (success) {
                log_info(3, "send msg success!");
                success = false;
            } else {
                log_err("send msg failed[msg: %lx]", (unsigned long)&msg);
                printmsg(&msg);
            }

            /* step 4: update snd_seq */
            comm_manager.snd_seq[msg.topid]++;
        }
    }
}

/**
 * @brief outsend - send msg to destination host
 *
 * @param msg msg to send
 * @return int
 */
static int outsend(jia_msg_t *msg) {
    log_out(3, "enter outsend");
    if (msg == NULL) {
        perror("msg is NULL");
        return -1;
    }

#ifdef DOSTAT
    STATOP(if (msg->size > 4096) jiastat.largecnt++;
            if (msg->size < 128) jiastat.smallcnt++;)
#endif

    int ret;
    struct sockaddr_in to_addr;
    ack_t ack;

    if (msg->topid == msg->frompid) {
        log_out(3, "equal pid");
        return enqueue(&inqueue, msg);
    } else {

#ifdef DOSTAT
        if (statflag == 1) {
            jiastat.msgsndcnt++;
            jiastat.msgsndbytes +=
                (outqueue.queue[outqueue.head].msg.size + Msgheadsize);
        }
#endif
        log_out(3, "not equal pid");
        /* step 1: send msg to destination host with ip */
        to_addr.sin_family = AF_INET;
        to_addr.sin_port = htons(comm_manager.snd_server_port);
        to_addr.sin_addr.s_addr =
            inet_addr(system_setting.hosts[msg->topid].ip);
        log_out(3, "snd_server_port is %u", comm_manager.snd_server_port);
        log_out(3, "toproc IP address is %u, IP port is %u",
                 to_addr.sin_addr.s_addr, to_addr.sin_port);
        sendto(comm_manager.snd_fds, msg, sizeof(jia_msg_t), 0,
               (struct sockaddr *)&to_addr, sizeof(struct sockaddr));

        log_out(3, "send once");
        /* step 2: wait for ack with time */
        FD_ZERO(&fdset);
        FD_SET(comm_manager.ack_fds, &fdset);
        ret = select(1, &fdset, NULL, NULL, &timeout);
        if ((ret == 1) &&  FD_ISSET(comm_manager.ack_fds, &fdset)) {
            log_out(3, "resend");
            ret = recvfrom(comm_manager.ack_fds, (char *)&ack, sizeof(ack_t), 0,
                           NULL, NULL);
            /* step 3: ack success && error manager*/
            if (ret != -1 && (ack.seqno == (msg->seqno + 1))) {
                return 0;
            }
            if (ret == -1) {
                log_info(3, "TIMEOUT! try resend");
                return -1;
            }
            // this cond may not happen
            if (ack.seqno != (msg->seqno + 1)) {
                log_out(3, "ERROR: seqno not match[ack.seqno: %d msg.seqno: %d]",
                         ack.seqno, msg->seqno);
                return -1;
            }
        }
    }

    return -1;
}
