#include "msg.h"
#include "rdma.h"
#include "setting.h"
#include "stat.h"
#include "tools.h"
#include <infiniband/verbs.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

pthread_t rdma_client_tid;
static struct ibv_wc wc;
static struct ibv_send_wr *bad_wr = NULL;
static jia_msg_t *msg_ptr;

static int post_send();

void *rdma_client_thread(void *arg) {
    while (1) {
        /* step 0: get sem value to print */
        int semvalue;
        // wait for busy slot
        sem_wait(&ctx.outqueue->busy_count);
        if(sem_getvalue(&ctx.outqueue->busy_count, &semvalue) != 0){
            log_err("sem wait fault");
        }
        log_info(4, "enter client outqueue dequeue! busy_count value: %d",
                 semvalue);

        /* step 1: give seqno */
        msg_ptr = (jia_msg_t *)(ctx.outqueue->queue[ctx.outqueue->head]);
        msg_ptr->seqno = ctx.connect_array[msg_ptr->topid].snd_seq;

        /* step 2: post send mr */
        if (msg_ptr->topid == system_setting.jia_pid) {

            /* step 1: memcpy outqueue to inqueue */
            msg_queue_t *inqueue = ctx.connect_array[msg_ptr->topid].inqueue;
            memcpy(inqueue->queue[inqueue->tail], msg_ptr, sizeof(jia_msg_t));

            log_info(3, "pre inqueue [tail]: %d, [busy_value]: %d [post_value]: %d", inqueue->tail,
                     atomic_load(&inqueue->busy_value), atomic_load(&inqueue->post_value));

            /* step 2: sub free_value and add busy_value */
            if (atomic_load(&(inqueue->free_value)) <= 0) {
                log_err("busy value error <= 0");
            } else {
                atomic_fetch_sub(&(inqueue->free_value), 1);
            }
            atomic_fetch_add(&(inqueue->busy_value), 1);

            /* step 3: update tail point */
            pthread_mutex_lock(&inqueue->tail_lock);
            inqueue->tail = (inqueue->tail + 1) % QueueSize;
            pthread_mutex_unlock(&inqueue->tail_lock);

            log_info(3, "after inqueue [tail]: %d, [busy_value]: %d [post_value]: %d",
                     inqueue->tail, atomic_load(&inqueue->busy_value), atomic_load(&inqueue->post_value));

        } else {
            while (post_send(&ctx.connect_array[msg_ptr->topid])) {
                log_info(3,
                         "msg <from:%d, to:%d, seq:%d, data:%s> send failed\n",
                         msg_ptr->frompid, msg_ptr->topid, msg_ptr->seqno,
                         msg_ptr->data);
            }
        }
#ifdef DOSTAT
        if (statflag == 1) {
            jiastat.msgsndcnt++;
            jiastat.msgsndbytes +=
                (((jia_msg_t *)outqueue.queue[outqueue.head])->size +
                 Msgheadsize);
        }
#endif

        log_info(3, "msg <from:%d, to:%d, seq:%d, data:%s> send successfully\n",
                 msg_ptr->frompid, msg_ptr->topid, msg_ptr->seqno,
                 msg_ptr->data);

        /* step 3: update snd_seq and head ptr */
        ctx.connect_array[msg_ptr->topid].snd_seq++;
        ctx.outqueue->head = (ctx.outqueue->head + 1) % QueueSize;

        /* step 4: sem post and print value */
        sem_post(&ctx.outqueue->free_count);
        sem_getvalue(&ctx.outqueue->free_count, &semvalue);
        log_info(4, "after client outqueue dequeue free_count value: %d",
                 semvalue);
    }
}

int post_send() {
    // TODO: we need to post different Send wr according to the msg's op

    struct ibv_cq *cq_ptr = NULL;
    void *context = NULL;
    int ret = -1;

    /* step 1: init wr, sge, for rdma to send */
    struct ibv_sge sge = {.addr =
                              (uint64_t)ctx.out_mr[ctx.outqueue->head]->addr,
                          .length = ctx.out_mr[ctx.outqueue->head]->length,
                          .lkey = ctx.out_mr[ctx.outqueue->head]->lkey};

    struct ibv_send_wr wr = {
        .wr_id = ctx.connect_array[msg_ptr->topid].snd_seq,
        .sg_list = &sge,
        .num_sge = 1,
        .next = NULL,
        .opcode = IBV_WR_SEND,
        .send_flags = IBV_SEND_SIGNALED,
    };

    /* step 2: loop until ibv_post_send wr successfully */
    jia_msg_t *msg_ptr = (jia_msg_t *)ctx.outqueue->queue[ctx.outqueue->head];
    while (
        ibv_post_send(ctx.connect_array[msg_ptr->topid].id.qp, &wr, &bad_wr)) {
        log_err("Failed to post send");
    }

    log_info(4, "post send wr successfully!");

    /* step 3: blocking get event */
    ret = ibv_get_cq_event(ctx.send_comp_channel, /* IO channel where we are
                                                   * expecting the notification
                                                   */
                           &cq_ptr, /* which CQ has an activity. This should be
                                       the same as CQ we created before */
                           &context); /* Associated CQ user context, which we
                                         did set on create CQ */
    if (ret) {
        log_err("Failed to get next CQ event due to %d \n", -errno);
        return -errno;
    }

    /* step 4: notify cq to get event */
    ret = ibv_req_notify_cq(cq_ptr, 0);
    if (ret) {
        log_err("Failed to request further notifications %d \n", -errno);
        return -errno;
    }

    /* step 5: check if we send the packet to fabric */
    while (1) {
        int ne = ibv_poll_cq(ctx.connect_array[msg_ptr->topid].id.qp->send_cq,
                             1, &wc);
        if (ne < 0) {
            log_err("ibv_poll_cq failed");
            return -1;
        } else if (ne == 0) {
            continue;
        } else {
            break;
        }
    }

    /* step 6: check wc.status and ack event */
    if (wc.status != IBV_WC_SUCCESS) {
        log_err("Failed status %s (%d) for wr_id %d",
                ibv_wc_status_str(wc.status), wc.status, (int)wc.wr_id);
        return -1;
    }
    ibv_ack_cq_events(cq_ptr, 
		       1 /* we received one event notification. This is not 
		       number of WC elements */);

    return 0;
}