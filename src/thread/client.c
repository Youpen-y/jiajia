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
#include <unistd.h>

#define RETRYNUM 4
static bool success = false;

pthread_t client_tid;
void *client_thread(void *args) {
    msg_queue_t *outqueue = (msg_queue_t *)args;
    jia_msg_t msg;
    ack_t ack;
    int ret;

    while (1) {
        if (dequeue(outqueue, &msg) != 0) {
            VERBOSE_LOG(3, "outqueue is null");
            continue;
        } else {
            /* step 1: give seqno */
            msg.seqno = comm_manager.snd_seq[msg.topid];

            /* step 2: send msg && ack */
            for (int retries_num = 0; retries_num < RETRYNUM; retries_num++) {
                if (!outsend(&msg)){
                    success = true;
                    break;
                }
            }

            /* step 3: manage error */
            if(success) {
                log_info(3, "send msg success!");
                success = false;
            }else{
                log_err("send msg failed[msg: %lx]", (unsigned long)&msg);
                printmsg(&msg);
            }

            /* step 4: update snd_seq */
            comm_manager.snd_seq[msg.topid]++;
        }
    }
}

int outsend(jia_msg_t *msg) {
    if (msg == NULL) {
        perror("msg is NULL");
        return -1;
    }

    int to_id, from_id, ret;
    to_id = msg->topid;
    from_id = msg->frompid;

    int sockfd = comm_manager.snd_fds[0];
    struct sockaddr_in to_addr;
    ack_t ack;

    if (to_id == from_id) {
        return enqueue(&inqueue, msg);
    } else {

#ifdef DOSTAT
        if (statflag == 1) {
            jiastat.msgsndcnt++;
            jiastat.msgsndbytes +=
                (outqueue.queue[outqueue.head].msg.size + Msgheadsize);
        }
#endif

        /* step 1: send msg to destination host with ip */
        to_addr.sin_family = AF_INET;
        to_addr.sin_port = htons(comm_manager.snd_server_port);
        to_addr.sin_addr.s_addr = inet_addr(system_setting.hosts[to_id].ip);
        log_info(3, "toproc IP address is %u, IP port is %u",
                    to_addr.sin_addr.s_addr, to_addr.sin_port);
        sendto(sockfd, msg, sizeof(jia_msg_t), 0, (struct sockaddr *)&to_addr,
               sizeof(struct sockaddr));

        /* step 2: wait for ack with time */
        unsigned long timeend = jia_current_time() + TIMEOUT;
        while ((jia_current_time() < timeend) && (errno == EWOULDBLOCK)) {
            ret = recvfrom(comm_manager.ack_fds, (char *)&ack, sizeof(ack_t), 0,
                           NULL, NULL);
        }

        /* step 3: ack success&&error manager*/
        if (ret != -1 && (ack.seqno == msg->seqno)) {
            return 0;
        }
        if (ret == -1) {
            log_info(3, "TIMEOUT! ret resend");
            return -1;
        }
        if (ack.seqno != msg->seqno) {
            log_info(3,
                        "ERROR: seqno not match[ack.seqno: %d msg.seqno: %d]",
                        ack.seqno, msg->seqno);
            return -1;
        }
    }

    return 0;
}
