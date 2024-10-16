#if !defined(STAT_H)
#define STAT_H

#include "comm.h"

#ifdef DOSTAT
    typedef struct Stats {
        unsigned int kernelflag;

        unsigned int msgsndbytes;       /*msg send bytes*/
        unsigned int msgrcvbytes;       /*msg recv bytes*/
        unsigned int msgrcvcnt;         /*msg recv count*/
        unsigned int msgsndcnt;         /*msg send count*/
        unsigned int segvLcnt;          /*segv local count*/
        unsigned int segvRcnt;          /*segv remote count*/
        unsigned int usersigiocnt;      /*user SIGIO count*/
        unsigned int synsigiocnt;       /*syn SIGIO count*/
        unsigned int segvsigiocnt;      /*segv SIGIO count*/
        unsigned int sigiocnt;          /*total SIGIO count*/
        unsigned int barrcnt;           /*barriers count*/
        unsigned int lockcnt;           /*lock count*/
        unsigned int getpcnt;           /*getp msg count*/
        unsigned int diffcnt;           /*diff msg count*/
        unsigned int invcnt;            /*inv msg count*/
        unsigned int mwdiffcnt;         /*MWdiffs count*/
        unsigned int repROcnt;          /*Replaced RO pages*/
        unsigned int repRWcnt;          /*Replaced RW pages*/
        unsigned int migincnt;
        unsigned int migoutcnt;
        unsigned int resentcnt;

        unsigned int segvLtime;
        unsigned int segvRtime;
        unsigned int barrtime;
        unsigned int locktime;
        unsigned int unlocktime;
        unsigned int usersigiotime;
        unsigned int synsigiotime;
        unsigned int segvsigiotime;
        
        unsigned int endifftime;
        unsigned int dedifftime;
        
/*Follow used by Shi*/
        unsigned int asendtime;
        unsigned int difftime;
        unsigned int busytime;
        unsigned int datatime;
        unsigned int syntime;
        unsigned int othertime;
        unsigned int segvtime;
        unsigned int commtime;
        unsigned int commsofttime;
        unsigned int commhardtime;
        unsigned int largecnt;          /*large msg count*/
        unsigned int smallcnt;          /*small msg count*/
        unsigned int overlapsigiotime;
        unsigned int overlapsigiocnt;
        unsigned int waittime;
    } jiastat_t;
#endif


#ifdef DOSTAT
extern jiastat_t jiastat;
extern jiastat_t allstats[Maxhosts];
extern int statflag;
extern unsigned int interruptflag;
extern volatile int waitstat;
#endif

void statserver(jia_msg_t *rep);

void statgrantserver(jia_msg_t *req);

void clearstat();

#endif //!defined(STAT_H)