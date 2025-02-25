#if !defined(STAT_H)
#define STAT_H

#include "comm.h"

typedef struct Stats {
    unsigned int kernelflag;
    unsigned int msgsndbytes;  /*msg send bytes*/
    unsigned int msgrcvbytes;  /*msg recv bytes*/
    unsigned int msgrcvcnt;    /*msg recv count*/
    unsigned int msgsndcnt;    /*msg send count*/
    unsigned int segvLcnt;     /*segv local count*/
    unsigned int segvRcnt;     /*segv remote count*/
    unsigned int usersigiocnt; /*user SIGIO count*/
    unsigned int synsigiocnt;  /*syn SIGIO count*/
    unsigned int segvsigiocnt; /*segv SIGIO count*/
    unsigned int sigiocnt;     /*total SIGIO count*/

    unsigned int barrcnt;     /*barriers count*/
    unsigned int lockcnt;     /*lock count*/
    unsigned int getpcnt;     /*getp msg count*/
    unsigned int prefetchcnt; /*prefetch pages count*/
    unsigned int diffcnt;     /*diff msg count*/
    unsigned int invcnt;      /*inv msg count*/
    unsigned int mwdiffcnt;   /*MWdiffs count*/
    unsigned int repROcnt;    /*Replaced RO pages*/
    unsigned int repRWcnt;    /*Replaced RW pages*/
    unsigned int migincnt;    /*Mig page home in*/
    unsigned int migoutcnt;   /*Mig page home out*/
    unsigned int resendcnt;   /*Resend msg count*/

    // usec(microseconds)
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
    unsigned int difftime; /*endifftime + dedifftime*/

    unsigned int busytime;
    unsigned int datatime;
    unsigned int syntime;
    unsigned int othertime;
    unsigned int segvtime;
    unsigned int commtime; /*not use now*/
    unsigned int commsofttime;
    unsigned int commhardtime;
    unsigned int largecnt; /*large msg count*/
    unsigned int smallcnt; /*small msg count*/
    unsigned int waittime; /*not use now*/

    unsigned int inittime;
    unsigned int initset;
    unsigned int initcreat;
    unsigned int createdir;
    unsigned int copyfiles;
    unsigned int startprocs;
    unsigned int initmem;
    unsigned int initsyn;
    unsigned int initmsg;
    unsigned int initcomm;
    unsigned int initrdmacontext;
    unsigned int initrdmaconnection;
    unsigned int initrdmaresource;

    unsigned int alloctime;
    unsigned int exittime;
} jiastat_t;

extern jiastat_t jiastat;
extern jiastat_t allstats[Maxhosts];
extern int statflag;
extern unsigned int interruptflag;
extern _Atomic volatile int waitstat;

/* function declarations */

/**
 * @brief statserver -- stat msg server
 *
 * @param rep msg of type STAT
 */
void statserver(jia_msg_t *rep);

/**
 * @brief statgrantserver -- grant stat msg server
 *
 * @param req
 */
void statgrantserver(jia_msg_t *req);

/**
 * @brief clearstat -- clear stat
 *
 */
void clearstat();

#endif //! defined(STAT_H)