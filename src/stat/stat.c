#ifndef NULL_LIB

#include "stat.h"
#include "comm.h"
#include "setting.h"

jiastat_t jiastat;
jiastat_t allstats[Maxhosts];
int statflag;
unsigned int interruptflag = 0;
int statcnt=0;
volatile int waitstat;
extern jia_msg_t msgarray[Maxmsgs]; 
extern volatile int msgbusy[Maxmsgs]; 
extern int msgcnt;


/**
 * @brief statserver -- stat msg server
 * 
 * @param rep msg of type STAT
 */
void statserver(jia_msg_t *rep)
{ 
   int i;
   jia_msg_t *grant;
   jiastat_t *stat;
   unsigned int temp;


 assert((rep->op==STAT)&&(rep->topid==0),"Incorrect STAT Message!");

 stat = (jiastat_t*)rep->data;
 allstats[rep->frompid].msgsndbytes  = stat->msgsndbytes;
 allstats[rep->frompid].msgrcvbytes  = stat->msgrcvbytes;
 allstats[rep->frompid].msgsndcnt    = stat->msgsndcnt;
 allstats[rep->frompid].msgrcvcnt    = stat->msgrcvcnt;
 allstats[rep->frompid].segvRcnt     = stat->segvRcnt;
 allstats[rep->frompid].segvLcnt     = stat->segvLcnt;
 allstats[rep->frompid].sigiocnt     = stat->sigiocnt;
 allstats[rep->frompid].usersigiocnt = stat->usersigiocnt;
 allstats[rep->frompid].synsigiocnt  = stat->synsigiocnt;
 allstats[rep->frompid].segvsigiocnt = stat->segvsigiocnt;
 allstats[rep->frompid].overlapsigiocnt = stat->overlapsigiocnt;
 allstats[rep->frompid].barrcnt      = stat->barrcnt;
 allstats[rep->frompid].lockcnt      = stat->lockcnt;
 allstats[rep->frompid].getpcnt      = stat->getpcnt;
 allstats[rep->frompid].diffcnt      = stat->diffcnt;
 allstats[rep->frompid].invcnt       = stat->invcnt;
 allstats[rep->frompid].mwdiffcnt    = stat->mwdiffcnt;
 allstats[rep->frompid].repROcnt     = stat->repROcnt;
 allstats[rep->frompid].repRWcnt     = stat->repRWcnt;
 allstats[rep->frompid].migincnt     = stat->migincnt;
 allstats[rep->frompid].migoutcnt    = stat->migoutcnt;
 allstats[rep->frompid].resentcnt    = stat->resentcnt;

 allstats[rep->frompid].barrtime     = stat->barrtime;
 allstats[rep->frompid].segvRtime    = stat->segvRtime;
 allstats[rep->frompid].segvLtime    = stat->segvLtime;
 allstats[rep->frompid].locktime     = stat->locktime;
 allstats[rep->frompid].unlocktime   = stat->unlocktime;
 allstats[rep->frompid].synsigiotime = stat->synsigiotime;
 allstats[rep->frompid].segvsigiotime= stat->segvsigiotime;
 allstats[rep->frompid].overlapsigiotime= stat->overlapsigiotime;
 allstats[rep->frompid].usersigiotime= stat->usersigiotime;
 allstats[rep->frompid].endifftime   = stat->endifftime;
 allstats[rep->frompid].dedifftime   = stat->dedifftime;
 allstats[rep->frompid].asendtime    = stat->asendtime;

/* Follow used by Shi*/
 allstats[rep->frompid].largecnt    = stat->largecnt;
 allstats[rep->frompid].smallcnt    = stat->smallcnt;

 allstats[rep->frompid].commsofttime = stat->msgsndcnt*ALPHAsend+BETAsend*stat->msgsndbytes+\
                                       stat->msgrcvcnt*ALPHArecv+BETArecv*stat->msgrcvbytes;
 allstats[rep->frompid].commhardtime = stat->msgsndcnt*ALPHA+BETA*stat->msgsndbytes;

 allstats[rep->frompid].difftime = allstats[rep->frompid].endifftime+allstats[rep->frompid].dedifftime;
 allstats[rep->frompid].waittime    = stat->waittime;

/*End Shi*/

 statcnt++;

 printf("Stats received from %d[%d]\n", rep->frompid, statcnt);


 if (statcnt == system_setting.hostc) {
    statcnt = 0;
    clearstat();
    int k = free_msg_index();
    grant = &msgarray[k];
    msgcnt++;
    msgbusy[k] = 1;

    grant->frompid = system_setting.jia_pid;
    grant->size = 0;
    grant->op=STATGRANT;
    for(i=0; i<system_setting.hostc; i++) {
       grant->topid = i;
       asendmsg(grant);
    }
    freemsg(grant);
 }
}

void clearstat()
{
   memset((char *)&jiastat, 0, sizeof(jiastat));
}

void statgrantserver(jia_msg_t *req)
{
   assert((req->op==STATGRANT)&&(req->topid==system_setting.jia_pid),"Incorrect STATGRANT Message!");
   waitstat = 0;
}


#else  /* NULL_LIB */
#endif /* NULL_LIB */