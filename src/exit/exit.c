/***********************************************************************
 *                                                                     *
 *   The JIAJIA Software Distributed Shared Memory System              * 
 *                                                                     *
 *   Copyright (C) 1997 the Center of High Performance Computing       * 
 *   of Institute of Computing Technology, Chinese Academy of          *  
 *   Sciences.  All rights reserved.                                   * 
 *                                                                     *
 *   Permission to use, copy, modify and distribute this software      *
 *   is hereby granted provided that (1) source code retains these     *
 *   copyright, permission, and disclaimer notices, and (2) redistri-  *
 *   butions including binaries reproduce the notices in supporting    *
 *   documentation, and (3) all advertising materials mentioning       *
 *   features or use of this software display the following            *
 *   acknowledgement: ``This product includes software developed by    *
 *   the Center of High Performance Computing, Institute of Computing  *
 *   Technology, Chinese Academy of Sciences."                         * 
 *                                                                     *
 *   This program is distributed in the hope that it will be useful,   *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of    *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.              * 
 *                                                                     *
 *   Center of High Performance Computing requests users of this       *
 *   software to return to dsm@water.chpc.ict.ac.cn any                * 
 *   improvements that they make and grant CHPC redistribution rights. *    
 *                                                                     * 
 *            Author: Weiwu Hu, Weisong Shi, Zhimin Tang               *       
 * =================================================================== *
 *   This software is ported to SP2 by                                 *
 *                                                                     *
 *         M. Rasit Eskicioglu                                         *
 *         University of Alberta                                       *
 *         Dept. of Computing Science                                  *
 *         Edmonton, Alberta T6G 2H1 CANADA                            *
 * =================================================================== *
 **********************************************************************/

#ifndef NULL_LIB
#include "global.h"
#include "jia.h"        // jia_wait
#include "init.h"
#include "comm.h"       // asendmsg
#include "mem.h"
#include "setting.h"
#include "tools.h"      // newmsg, freemsg, appendmsg
#include "stat.h"       // clearstat

extern unsigned long globaladdr;
extern volatile int incount;
extern volatile int outcount;

/**
 * @brief jia_exit -- if defined DOSTAT, print statistic; else do nothing
 * 
 */
void jia_exit()
{
   int jia_pid = system_setting.jia_pid;
   int hostc = system_setting.hostc;
#ifdef DOSTAT
 int i;
 jia_msg_t *reply;
 jiastat_t *stat_p = &jiastat;
 jiastat_t total;


 jia_wait();

 printf("\nShared Memory (%d-byte pages) : %d (total) %d (used)\n", 
         Pagesize, Maxmemsize/Pagesize, globaladdr/Pagesize);
 if (hostc > 1) {

    reply = newmsg();
    reply->frompid = system_setting.jia_pid;
    reply->topid = 0;
    reply->size = 0;
    reply->op = STAT;
    appendmsg(reply, (char*)stat_p, sizeof(jiastat));
 
    waitstat = 1;
    asendmsg(reply);
    freemsg(reply);
    while(waitstat);

#ifdef DEBUG
printf("Print stats\n");
fflush(stdout);
#endif

   fclose(logfile);
   /*Follow used by Shi*/
    if (system_setting.jia_pid == 0) {
       memset((char*)&total, 0, sizeof(total)); 
       for (i=0; i<hostc; i++) {
         total.msgsndcnt   += allstats[i].msgsndcnt;
         total.msgrcvcnt   += allstats[i].msgrcvcnt;
         total.msgsndbytes += allstats[i].msgsndbytes;
         total.msgrcvbytes += allstats[i].msgrcvbytes;
         total.segvLcnt    += allstats[i].segvLcnt;
         total.segvRcnt    += allstats[i].segvRcnt;
         total.barrcnt      = allstats[i].barrcnt;
         total.lockcnt     += allstats[i].lockcnt;
         total.getpcnt     += allstats[i].getpcnt;
         total.diffcnt     += allstats[i].diffcnt;
         total.invcnt      += allstats[i].invcnt;
         total.mwdiffcnt   += allstats[i].mwdiffcnt;
         total.repROcnt    += allstats[i].repROcnt;
         total.repRWcnt    += allstats[i].repRWcnt;
         total.migincnt    += allstats[i].migincnt;
         total.migoutcnt    += allstats[i].migoutcnt;
         total.resentcnt    += allstats[i].resentcnt;

         total.usersigiocnt+= allstats[i].usersigiocnt;
         total.synsigiocnt += allstats[i].synsigiocnt;
         total.segvsigiocnt+= allstats[i].segvsigiocnt;
         total.overlapsigiocnt+= allstats[i].overlapsigiocnt;

         total.segvLtime   += allstats[i].segvLtime;
         total.segvRtime   += allstats[i].segvRtime;
         total.barrtime    += allstats[i].barrtime;
         total.locktime    += allstats[i].locktime;
         total.unlocktime  += allstats[i].unlocktime;
         total.usersigiotime += allstats[i].usersigiotime;
         total.synsigiotime  += allstats[i].synsigiotime;
         total.segvsigiotime += allstats[i].segvsigiotime;
         total.overlapsigiotime += allstats[i].overlapsigiotime;
      
         total.largecnt      += allstats[i].largecnt;
         total.smallcnt      += allstats[i].smallcnt;
         total.syntime     += allstats[i].syntime;
         total.commsofttime  += allstats[i].commsofttime;
         total.commhardtime  += allstats[i].commhardtime;
         total.difftime    += allstats[i].difftime;
         total.waittime    += allstats[i].waittime;
       }	
    /*end Shi*/

       for (i=0; i<hostc*9/2-1; i++) printf("#");
       printf("  JIAJIA STATISTICS  ");
       for (i=0; i<hostc*9/2; i++) printf("#");
       if (hostc%2) printf("#");
       printf("\n           hosts --> ");
       for (i=0; i<hostc; i++) printf("%8d ", i);
       printf("     total");
       printf("\n");
       for (i=0; i<20+hostc*9; i++) printf("-");
       printf("\nMsgs Sent          = ");
       for (i=0; i<hostc; i++) printf("%8d ", allstats[i].msgsndcnt); 
       printf(" %8d ", total.msgsndcnt);
       printf("\nMsgs Received      = ");
       for (i=0; i<hostc; i++) printf("%8d ", allstats[i].msgrcvcnt);
       printf(" %8d ", total.msgrcvcnt);
       printf("\nBytes Sent         = ");
       for (i=0; i<hostc; i++) printf("%8d ", allstats[i].msgsndbytes);
       printf(" %8d ", total.msgsndbytes);
       printf("\nBytes Received     = ");
       for (i=0; i<hostc; i++) printf("%8d ", allstats[i].msgrcvbytes);
       printf(" %8d ", total.msgrcvbytes);
       printf("\nSEGVs (local)      = ");
       for (i=0; i<hostc; i++) printf("%8d ", allstats[i].segvLcnt);
       printf(" %8d ", total.segvLcnt);
       printf("\nSEGVs (remote)     = ");
       for (i=0; i<hostc; i++) printf("%8d ", allstats[i].segvRcnt);
       printf(" %8d ", total.segvRcnt);
       printf("\nSIGIOs (total)     = ");
       for (i=0; i<hostc; i++) printf("%8d ", allstats[i].sigiocnt);
       printf(" %8d ", total.sigiocnt);
       printf("\nSIGIOs (user)      = ");
       for (i=0; i<hostc; i++) printf("%8d ", allstats[i].usersigiocnt);
       printf(" %8d ", total.usersigiocnt);
       printf("\nSIGIOs (Syn.)      = ");
       for (i=0; i<hostc; i++) printf("%8d ", allstats[i].synsigiocnt);
       printf(" %8d ", total.synsigiocnt);
       printf("\nSIGIOs (SEGV)      = ");
       for (i=0; i<hostc; i++) printf("%8d ", allstats[i].segvsigiocnt);
       printf(" %8d ", total.segvsigiocnt);
       printf("\nBarriers           = ");
       for (i=0; i<hostc; i++) printf("%8d ", allstats[i].barrcnt);
       printf(" %8d ", total.barrcnt);
       printf("\nLocks              = ");
       for (i=0; i<hostc; i++) printf("%8d ", allstats[i].lockcnt);
       printf(" %8d ", total.lockcnt);
       printf("\nGetp Reqs          = ");
       for (i=0; i<hostc; i++) printf("%8d ", allstats[i].getpcnt);
       printf(" %8d ", total.getpcnt);
       printf("\nDiff Msgs.         = ");
       for (i=0; i<hostc; i++) printf("%8d ", allstats[i].diffcnt);
       printf(" %8d ", total.diffcnt);
       printf("\nMWdiffs            = ");
       for (i=0; i<hostc; i++) printf("%8d ", allstats[i].mwdiffcnt);
       printf(" %8d ", total.mwdiffcnt);
       printf("\nInvalidate         = ");
       for (i=0; i<hostc; i++) printf("%8d ", allstats[i].invcnt);
       printf(" %8d ", total.invcnt);
       printf("\nReplaced RO pages  = ");
       for (i=0; i<hostc; i++) printf("%8d ", allstats[i].repROcnt);
       printf(" %8d ", total.repROcnt);
       printf("\nReplaced RW pages  = ");
       for (i=0; i<hostc; i++) printf("%8d ", allstats[i].repRWcnt);
       printf(" %8d ", total.repRWcnt);
       printf("\nMig in pages       = ");
       for (i=0; i<hostc; i++) printf("%8d ", allstats[i].migincnt);
       printf(" %8d ", total.migincnt);
       printf("\nMig out pages      = ");
       for (i=0; i<hostc; i++) printf("%8d ", allstats[i].migoutcnt);
       printf(" %8d ", total.migoutcnt);

       printf("\nResent Msgs        = ");
       for (i=0; i<hostc; i++) printf("%8d ", allstats[i].resentcnt);
       printf(" %8d ", total.resentcnt);

       printf("\nLarge message      = ");
       for (i=0; i<hostc; i++) printf("%8d ", allstats[i].largecnt);
       printf(" %8d ", total.largecnt);
       printf("\nSmall message      = ");
       for (i=0; i<hostc; i++) printf("%8d ", allstats[i].smallcnt);
       printf(" %8d ", total.smallcnt);

       printf("\n");
       for (i=0; i<20+hostc*9; i++) printf("-");
       printf(" (ms) ");
       printf("\nBarrier time       = ");
       for (i=0; i<hostc; i++) printf("%8.2f ", allstats[i].barrtime/1000.0);
       printf("\nLock time          = ");
       for (i=0; i<hostc; i++) printf("%8.2f ", allstats[i].locktime/1000.0);
       printf("\nUnlock time        = ");
       for (i=0; i<hostc; i++) printf("%8.2f ", allstats[i].unlocktime/1000.0);
       printf("\nSEGV time (local)  = ");
       for (i=0; i<hostc; i++) printf("%8.2f ", allstats[i].segvLtime/1000.0);
       printf("\nSEGV time (remote) = ");
       for (i=0; i<hostc; i++) printf("%8.2f ", allstats[i].segvRtime/1000.0);
       printf("\nSIGIO time (user)  = ");
       for (i=0; i<hostc; i++) printf("%8.2f ", allstats[i].usersigiotime/1000.0);
       printf("\nSIGIO time (Syn.)  = ");
       for (i=0; i<hostc; i++) printf("%8.2f ", allstats[i].synsigiotime/1000.0);
       printf("\nSIGIO time (SEGV)  = ");
       for (i=0; i<hostc; i++) printf("%8.2f ", allstats[i].segvsigiotime/1000.0);
       printf("\nSIGIO time (Over)  = ");
       for (i=0; i<hostc; i++) printf("%8.2f ", allstats[i].overlapsigiotime/1000.0);
       printf("\nEncode diff time   = ");
       for (i=0; i<hostc; i++) printf("%8.2f ", allstats[i].endifftime/1000.0);
       printf("\nDecode diff time   = ");
       for (i=0; i<hostc; i++) printf("%8.2f ", allstats[i].dedifftime/1000.0);
       printf("\nAsendmsg time      = ");
       for (i=0; i<hostc; i++) printf("%8.2f ", allstats[i].asendtime/1000.0);
       //printf("\ncomm soft time     = ");
       //for (i=0; i<hostc; i++) printf("%8.2f ", allstats[i].commsofttime/1000.0);
       //printf("\ncomm hard time     = ");
       //for (i=0; i<hostc; i++) printf("%8.2f ", allstats[i].commhardtime/1000.0);
       printf("\nWait time          = ");
       for (i=0; i<hostc; i++) printf("%8.2f ", allstats[i].waittime/1000.0);

       printf("\n");
       for (i=0; i<20+hostc*9; i++) printf("-");

       printf("\nSEGV time          = ");
       for (i=0; i<hostc; i++) printf("%8.2f ", (allstats[i].segvLtime+allstats[i].segvRtime+
           allstats[i].segvLcnt*SEGVoverhead+allstats[i].segvRcnt*SEGVoverhead-
           allstats[i].segvsigiotime-allstats[i].segvsigiocnt*SIGIOoverhead)/1000.0);
       printf("\nSyn. time          = ");
       for (i=0; i<hostc; i++) printf("%8.2f ", (allstats[i].barrtime+allstats[i].locktime+
           allstats[i].unlocktime- allstats[i].synsigiotime-
           allstats[i].synsigiocnt*SIGIOoverhead)/1000.0);
       printf("\nServer time        = ");
       for (i=0; i<hostc; i++) printf("%8.2f ", (allstats[i].usersigiotime+allstats[i].synsigiotime+
           allstats[i].segvsigiotime+(allstats[i].usersigiocnt+allstats[i].synsigiocnt+
           allstats[i].segvsigiocnt)*SIGIOoverhead)/1000.0);

       printf("\n");
       for (i=0; i<20+hostc*9; i++) printf("#");
       printf("\n");
    }
  }
#endif /* DOSTAT */
}
#else  /* NULL_LIB */
void jia_exit()
{
  exit(0);
} 
#endif /* NULL_LIB */
