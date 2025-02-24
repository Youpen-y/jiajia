#include "global.h"
#include "msg.h"
#include "rdma.h"
#include "setting.h"
#include "stat.h"
#include "tools.h"
#include <infiniband/verbs.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdatomic.h>
#include <stdlib.h>

#define CQID(cq_ptr)                                                           \
    (((void *)cq_ptr - (void *)ctx.connect_array) / sizeof(rdma_connect_t))

pthread_t rdma_listen_tid;
static struct ibv_wc wc;
static int post_recv();

void *rdma_listen_thread(void *arg) {
    post_recv();

    return NULL;
}

int post_recv() {
    unsigned int cqid;
    msg_queue_t *inqueue;
    struct ibv_cq *cq_ptr = NULL;
    void *context = NULL;
    int ret = -1;
    while (1) {
        /* We wait for the notification on the CQ channel */
        ret = ibv_get_cq_event(
            ctx.recv_comp_channel, /* IO channel where we are expecting the notification
                           */
            &cq_ptr, /* which CQ has an activity. This should be the same as CQ
                        we created before */
            &context); /* Associated CQ user context, which we did set */
        if (ret) {
            log_err("Failed to get next CQ event due to %d \n", -errno);
            return -errno;
        }

        /* ensure cqid and inqueue*/
        inqueue = (*(rdma_connect_t *)context).inqueue;

        /* Request for more notifications. */
        ret = ibv_req_notify_cq(cq_ptr, 0);
        if (ret) {
            log_err("Failed to request further notifications %d \n", -errno);
            return -errno;
        }

        /* We got notification. We reap the work completion (WC) element. It is
         * unlikely but a good practice it write the CQ polling code that
         * can handle zero WCs. ibv_poll_cq can return zero. Same logic as
         * MUTEX conditional variables in pthread programming.
         */
        ret = ibv_poll_cq(cq_ptr /* the CQ, we got notification for */,
                          1 /* number of remaining WC elements*/,
                          &wc /* where to store */);
        if (ret < 0) {
            log_err("Failed to poll cq for wc due to %d \n", ret);
            /* ret is errno here */
            return ret;
        } else if (ret == 0) {
            continue;
        }
        log_info(3, "%d WC are completed \n", ret);

        /* Now we check validity and status of I/O work completions */
        if (wc.status != IBV_WC_SUCCESS) {
            log_err("Work completion (WC) has error status: %s",
                    ibv_wc_status_str(wc.status));

            switch (wc.status) {
            case IBV_WC_RNR_RETRY_EXC_ERR:
                // receive side is not ready and exceed the retry count
                log_err("Remote endpoint not ready, retry exceeded\n");
                break;

            case IBV_WC_RETRY_EXC_ERR:
                // sender exceed retry count
                log_err("Transport retry count exceeded\n");
                break;

            case IBV_WC_LOC_LEN_ERR:
                // local length error
                log_err("Local length error\n");
                break;

            case IBV_WC_LOC_QP_OP_ERR:
                // QP operation error
                log_err("Local QP operation error\n");
                break;

            case IBV_WC_REM_ACCESS_ERR:
                // remote access error
                log_err("Remote access error\n");
                break;

            default:
                log_err("Unhandled error status\n");
                break;
            }
        } else {
#ifdef DOSTAT
            if (statflag == 1) {
                jiastat.msgrcvcnt++;
                jiastat.msgrcvbytes +=
                    (((jia_msg_t *)inqueue->queue[inqueue->tail])->size +
                     Msgheadsize);
            }
#endif

            log_info(
                3, "pre inqueue [tail]: %d, [busy_value]: %d [post_value]: %d",
                inqueue->tail, atomic_load(&inqueue->busy_value),
                atomic_load(&inqueue->post_value));

            /* step 1: sub post_value and add busy_value */
            if (atomic_load(&(inqueue->post_value)) <= 0) {
                log_err("post value error <= 0");
            } else {
                atomic_fetch_sub(&(inqueue->post_value), 1);
            }
            atomic_fetch_add(&(inqueue->busy_value), 1);

            /* step 2: update tail */
            pthread_mutex_lock(&inqueue->tail_lock);
            inqueue->tail = (inqueue->tail + 1) % QueueSize;
            pthread_mutex_unlock(&inqueue->tail_lock);

            log_info(
                3,
                "after inqueue [tail]: %d, [busy_value]: %d [post_value]: %d",
                inqueue->tail, atomic_load(&inqueue->busy_value),
                atomic_load(&inqueue->post_value));
        }

        /* Similar to connection management events, we need to acknowledge CQ
         * events
         */
        ibv_ack_cq_events(cq_ptr, 
		       1 /* we received one event notification. This is not 
		       number of WC elements */);
    }
}
