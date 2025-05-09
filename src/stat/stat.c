#ifndef NULL_LIB
#include "stat.h"
#include "comm.h"
#include "msg.h"
#include "setting.h"
#include "tools.h"
#include <stdatomic.h>

jiastat_t jiastat;
jiastat_t allstats[Maxhosts];
int statflag;
unsigned int interruptflag = 0;
int statcnt = 0;
_Atomic volatile int waitstat;

/**
 * @brief statserver -- stat msg server
 *
 * @param rep msg of type STAT
 */
void statserver(jia_msg_t *rep) {
    int i;
    jia_msg_t *grant;
    jiastat_t *stat;
    unsigned int temp;

    jia_assert((rep->op == STAT) && (rep->topid == 0),
               "Incorrect STAT Message!");

    stat = (jiastat_t *)rep->data;
    allstats[rep->frompid].msgsndbytes = stat->msgsndbytes;
    allstats[rep->frompid].msgrcvbytes = stat->msgrcvbytes;
    allstats[rep->frompid].msgsndcnt = stat->msgsndcnt;
    allstats[rep->frompid].msgrcvcnt = stat->msgrcvcnt;
    allstats[rep->frompid].segvRcnt = stat->segvRcnt;
    allstats[rep->frompid].segvLcnt = stat->segvLcnt;
    allstats[rep->frompid].sigiocnt = stat->sigiocnt;
    allstats[rep->frompid].usersigiocnt = stat->usersigiocnt;
    allstats[rep->frompid].synsigiocnt = stat->synsigiocnt;
    allstats[rep->frompid].segvsigiocnt = stat->segvsigiocnt;
    allstats[rep->frompid].barrcnt = stat->barrcnt;
    allstats[rep->frompid].lockcnt = stat->lockcnt;
    allstats[rep->frompid].getpcnt = stat->getpcnt;
    allstats[rep->frompid].prefetchcnt = stat->prefetchcnt;
    allstats[rep->frompid].diffcnt = stat->diffcnt;
    allstats[rep->frompid].invcnt = stat->invcnt;
    allstats[rep->frompid].mwdiffcnt = stat->mwdiffcnt;
    allstats[rep->frompid].repROcnt = stat->repROcnt;
    allstats[rep->frompid].repRWcnt = stat->repRWcnt;
    allstats[rep->frompid].migincnt = stat->migincnt;
    allstats[rep->frompid].migoutcnt = stat->migoutcnt;
    allstats[rep->frompid].resendcnt = stat->resendcnt;

    allstats[rep->frompid].barrtime = stat->barrtime;
    allstats[rep->frompid].segvRtime = stat->segvRtime;
    allstats[rep->frompid].segvLtime = stat->segvLtime;
    allstats[rep->frompid].locktime = stat->locktime;
    allstats[rep->frompid].unlocktime = stat->unlocktime;
    allstats[rep->frompid].synsigiotime = stat->synsigiotime;
    allstats[rep->frompid].segvsigiotime = stat->segvsigiotime;
    allstats[rep->frompid].usersigiotime = stat->usersigiotime;
    allstats[rep->frompid].endifftime = stat->endifftime;
    allstats[rep->frompid].dedifftime = stat->dedifftime;

    /* Follow used by Shi*/
    allstats[rep->frompid].largecnt = stat->largecnt;
    allstats[rep->frompid].smallcnt = stat->smallcnt;

    allstats[rep->frompid].commsofttime =
        stat->msgsndcnt * ALPHAsend + BETAsend * stat->msgsndbytes +
        stat->msgrcvcnt * ALPHArecv + BETArecv * stat->msgrcvbytes;
    allstats[rep->frompid].commhardtime =
        stat->msgsndcnt * ALPHA + BETA * stat->msgsndbytes;

    allstats[rep->frompid].difftime =
    allstats[rep->frompid].endifftime + allstats[rep->frompid].dedifftime;
    allstats[rep->frompid].waittime = stat->waittime;
    allstats[rep->frompid].inittime = stat->inittime;
    allstats[rep->frompid].initset = stat->initset;
    allstats[rep->frompid].initcreat = stat->initcreat;
    allstats[rep->frompid].createdir = stat->createdir;
    allstats[rep->frompid].copyfiles = stat->copyfiles;
    allstats[rep->frompid].startprocs = stat->startprocs;
    allstats[rep->frompid].initmem = stat->initmem;
    allstats[rep->frompid].initcomm = stat->initcomm;
    allstats[rep->frompid].initrdmacontext = stat->initrdmacontext;
    allstats[rep->frompid].initrdmaconnection = stat->initrdmaconnection;
    allstats[rep->frompid].initrdmaresource = stat->initrdmaresource;
    allstats[rep->frompid].initmsg = stat->initmsg;

    allstats[rep->frompid].alloctime = stat->alloctime;

    /*End Shi*/

    statcnt++;

    log_info(3, "Stats received from %d[%d]\n", rep->frompid, statcnt);

    if (statcnt == system_setting.hostc) {
        statcnt = 0;
        clearstat();
        slot_t* slot = freemsg_lock(&msg_buffer);
        grant = &(slot->msg);
        grant->frompid = system_setting.jia_pid;
        grant->size = 0;
        grant->op = STATGRANT;
        for (i = system_setting.hostc-1; i >= 0; i--) {
            grant->topid = i;
            move_msg_to_outqueue(slot, &outqueue);
        }
        freemsg_unlock(slot);
    }
}

void clearstat() {
    memset((char *)&jiastat, 0, sizeof(jiastat));
}

void statgrantserver(jia_msg_t *req) {
    jia_assert((req->op == STATGRANT) && (req->topid == system_setting.jia_pid),
               "Incorrect STATGRANT Message!");

    atomic_store(&waitstat, 0);
}

#else  /* NULL_LIB */
#endif /* NULL_LIB */