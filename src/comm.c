/**********************************************************************
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
 *         Author: Weiwu Hu, Weisong Shi, Zhimin Tang                  *
 * =================================================================== *
 *   This software is ported to SP2 by                                 *
 *                                                                     *
 *         M. Rasit Eskicioglu                                         *
 *         University of Alberta                                       *
 *         Dept. of Computing Science                                  *
 *         Edmonton, Alberta T6G 2H1 CANADA                            *
 * =================================================================== *
 **********************************************************************/

#include "utils.h"
#ifndef NULL_LIB
#include "comm.h"
#include "mem.h"
#include "tools.h"

#define BEGINCS                                                                \
    {                                                                          \
        sigset_t newmask, oldmask;                                             \
        sigemptyset(&newmask);                                                 \
        sigaddset(&newmask, SIGIO);                                            \
        sigprocmask(SIG_BLOCK, &newmask, &oldmask);                            \
        oldsigiomask = sigismember(&oldmask, SIGIO);                           \
        printf("Enter CS\t");                                                  \
    }
#define ENDCS                                                                  \
    {                                                                          \
        if (oldsigiomask == 0)                                                 \
            enable_sigio();                                                    \
        printf("Exit CS\n");                                                   \
    }

// #ifndef JIA_DEBUG
// #define  msgprint  0
// #define  printf   emptyprintf
// #else  /* JIA_DEBUG */
// #define msgprint  1
// #endif  /* JIA_DEBUG */
// #define msgprint 1

/*----------------------------------------------------------*/
/* following definitions are defined by Hu */
extern host_t hosts[Maxhosts];
extern int jia_pid;
extern int hostc;
extern char errstr[Linesize];
extern int msgbusy[Maxmsgs];
extern jia_msg_t msgarray[Maxmsgs];
extern int msgcnt;

CommManager commreq, commrep;

/* head msg, tail msg, msg count in out queue */
volatile int inhead, intail, incount;
volatile int outhead, outtail, outcount;

// inqueue used to store in-msg, outqueue used to store out-msg
jia_msg_t inqueue[Maxqueue], outqueue[Maxqueue];

/* servers used by asynchronous */
extern void diffserver(jia_msg_t *);
extern void getpserver(jia_msg_t *);
extern void acqserver(jia_msg_t *);
extern void invserver(jia_msg_t *);
extern void relserver(jia_msg_t *);
extern void jiaexitserver(jia_msg_t *);
extern void wtntserver(jia_msg_t *);
extern void barrserver(jia_msg_t *);
extern void barrgrantserver(jia_msg_t *);
extern void acqgrantserver(jia_msg_t *);
extern void waitgrantserver(jia_msg_t *);
extern void waitserver(jia_msg_t *);
extern void diffgrantserver(jia_msg_t *);
extern void getpgrantserver(jia_msg_t *);
extern void loadserver(jia_msg_t *);
extern void loadgrantserver(jia_msg_t *);
extern void emptyprintf();

extern void setcvserver(jia_msg_t *);
extern void resetcvserver(jia_msg_t *);
extern void waitcvserver(jia_msg_t *);
extern void cvgrantserver(jia_msg_t *);

/* external function from msg.c*/
extern void msgrecvserver(jia_msg_t *);

extern void statserver(jia_msg_t *);
extern void statgrantserver(jia_msg_t *);

extern unsigned int get_usecs();

extern void printmsg(jia_msg_t *, int);
extern jia_msg_t *newmsg();

#ifdef DOSTAT
extern int statflag;
extern jiastat_t jiastat;
unsigned int interruptflag = 0;
#endif

/* following definitions are defined by Shi */
unsigned long reqports[Maxhosts]
                      [Maxhosts]; // every host has Maxhosts request ports
unsigned long repports[Maxhosts]
                      [Maxhosts]; // every host has Maxhosts reply ports
// commreq used to send and recv request, commrep used to send and
// recv reply
unsigned long timeout_time;
static struct timeval polltime = {0, 0};
long Startport;

// void    initcomm();
// int     req_fdcreate(int, int);
// int     rep_fdcreate(int, int);
// #if defined SOLARIS || defined IRIX62
// void    sigio_handler(int sig, siginfo_t *sip, ucontext_t *uap);
// #endif /* SOLARIS */
// #ifdef LINUX
// void    sigio_handler();
// #endif
// #ifdef AIX41
// void    sigio_handler();
// #endif /* AIX41 */
// void    sigint_handler();
// void    asendmsg(jia_msg_t *);
// void    msgserver();
// void    outsend();
// void bcastserver(jia_msg_t *msg);

extern unsigned long jia_current_time();
extern void disable_sigio();
extern void enable_sigio();

extern sigset_t oldset;
int oldsigiomask;

/*---------------------------------------------------------*/

/**
 * @brief inqrecv() -- recv msg (update incount&&intail)
 *
 * @return iff incount==1
 */
static inline int inqrecv(int fromproc) {
	assert0((incount < Maxqueue), "outsend(): Inqueue exceeded!");
    incount++;
    intail = (intail + 1) % Maxqueue;
	// update seqno from host fromproc
    commreq.rcv_seq[fromproc] = inqt.seqno;
    printmsg(&inqt, 1);
    return (incount == 1);
};

/**
 * @brief inqcomp() -- complete msg (update incount&&inhead)
 *
 * @return iff incount > 0
 */
static inline int inqcomp() {
    inqh.op = ERRMSG;
    inhead = (inhead + 1) % Maxqueue;
    incount--;
    return (incount > 0);
};

/**
 * @brief req_fdcreate -- creat socket file descriptor used to send and recv
 * request
 *
 * @param i the index of host
 * @param flag 1 means sin_port = 0, random port; others means specified
 * sin_port = reqports[jia_pid][i]
 * @return int socket file descriptor
 * creat socket file descriptor(fd) used to send and recv request and bind it to
 * an address (ip/port combination)
 */
int req_fdcreate(int i, int flag) {
    int fd, res;
    struct sockaddr_in addr;

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    assert0((fd != -1), "req_fdcreate()-->socket()");

#ifdef SOLARIS
    size = Maxmsgsize + Msgheadsize + 128;
    res = setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (char *)&size, sizeof(size));
    assert0((res == 0), "req_fdcreate()-->setsockopt():SO_RCVBUF");

    size = Maxmsgsize + Msgheadsize + 128;
    res = setsockopt(fd, SOL_SOCKET, SO_SNDBUF, (char *)&size, sizeof(size));
    assert0((res == 0), "req_fdcreate()-->setsockopt():SO_SNDBUF");
#endif
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = (flag) ? htons(0) : htons(reqports[jia_pid][i]);

    res = bind(fd, (struct sockaddr *)&addr, sizeof(addr));
    assert0((res == 0), "req_fdcreate()-->bind()");

    return fd;
}

/**
 * @brief rep_fdcreate -- create socket file descriptor(fd) used to send and
 * recv reply
 *
 * @param i the index of host [0, hostc)
 * @param flag equals to 1 means fd with random port, 0 means fd with specified
 * port(repports[jia_pid][i])
 * @return int socket file descriptor(fd)
 */
int rep_fdcreate(int i, int flag) {
    int fd, res;
#ifdef SOLARIS
    int size;
#endif /* SOLARIS */
    struct sockaddr_in addr;

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    assert0((fd != -1), "rep_fdcreate()-->socket()");

#ifdef SOLARIS
    size = Intbytes;
    res = setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (char *)&size, sizeof(size));
    assert0((res == 0), "rep_fdcreate()-->setsockopt():SO_RCVBUF");

    size = Intbytes;
    res = setsockopt(fd, SOL_SOCKET, SO_SNDBUF, (char *)&size, sizeof(size));
    assert0((res == 0), "rep_fdcreate()-->setsockopt():SO_SNDBUF");
#endif /* SOLARIS */

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = (flag) ? htons(0) : htons(repports[jia_pid][i]);

    res = bind(fd, (struct sockaddr *)&addr, sizeof(addr));
    assert0((res == 0), "rep_fdcreate()-->bind()");
    return fd;
}

/**
 * @brief initcomm -- initialize communication setting
 *
 * step1: initialize msg array and correpsonding flag to indicate busy or free
 *
 * step2: initialize pointer that indicate head, tail and count of inqueue and
 * outqueue
 *
 * step3: register signal handler (SIGIO, SIGINT)
 *
 * step4: initialize comm ports (reqports, repports)
 *
 * step5: initialize comm manager (commreq, commrep)
 */
void initcomm() {
    int i, j, fd;

    if (jia_pid == 0) {
        printf("************Initialize Communication!*******\n");
    }
    printf("current jia_pid = %d\n", jia_pid);
    printf(" Startport = %d \n", Startport);

    msgcnt = 0;
    for (i = 0; i < Maxmsgs; i++) {
        msgbusy[i] = 0;
        msgarray[i].index = i;
    }

    inhead = 0;
    intail = 0;
    incount = 0;
    outhead = 0;
    outtail = 0;
    outcount = 0;

#if defined SOLARIS || defined IRIX62
    {
        struct sigaction act;

        act.sa_handler = (void_func_handler)sigio_handler;
        sigemptyset(&act.sa_mask);
        act.sa_flags = SA_SIGINFO;
        if (sigaction(SIGIO, &act, NULL))
            assert0(0, "initcomm()-->sigaction()");

        act.sa_handler = (void_func_handler)sigint_handler;
        sigemptyset(&act.sa_mask);
        act.sa_flags = SA_SIGINFO;
        if (sigaction(SIGINT, &act, NULL)) {
            assert0(0, "segv sigaction problem");
        }
    }
#endif
#ifdef LINUX
    {
        struct sigaction act;

        act.sa_handler = (void_func_handler)sigio_handler;
        sigemptyset(&act.sa_mask);
        act.sa_flags = SA_RESTART;
        if (sigaction(SIGIO, &act, NULL))
            assert0(0, "initcomm()-->sigaction()");

        act.sa_handler = (void_func_handler)sigint_handler;
        sigemptyset(&act.sa_mask);
        act.sa_flags = SA_NOMASK;
        if (sigaction(SIGINT, &act, NULL)) {
            assert0(0, "segv sigaction problem");
        }
    }
#endif
#ifdef AIX41
    {
        struct sigvec vec;

        vec.sv_handler = (void_func_handler)sigio_handler;
        vec.sv_flags = SV_INTERRUPT;
        sigvec(SIGIO, &vec, NULL);

        vec.sv_handler = (void_func_handler)sigint_handler;
        vec.sv_flags = 0;
        sigvec(SIGINT, &vec, NULL);
    }
#endif

    /***********Initialize comm ports********************/

    for (i = 0; i < Maxhosts; i++) {
        for (j = 0; j < Maxhosts; j++) {
            reqports[i][j] = Startport + i * Maxhosts + j;
            repports[i][j] = Startport + Maxhosts * Maxhosts + i * Maxhosts + j;
        }
    }

    /* output comm ports */
    // printf("reqports\t repports\n");
    //   for(i = 0; i < Maxhosts; i++){
    //     for(j = 0; j < Maxhosts; j++){
    //       printf("reqports[%d][%d] = %d\t", i, j, reqports[i][j]);
    //       printf("repports[%d][%d] = %d\n", i, j, repports[i][j]);
    //     }
    //   }

#ifdef JIA_DEBUG
    for (i = 0; i < Maxhosts; i++)
        for (j = 0; j < Maxhosts; j++) {
            if (j == 0)
                printf("\nREQ[%02d][] = ", i);
            else if (j % 5)
                printf("%d  ", reqports[i][j]);
            else
                printf("%d  \n            ", reqports[i][j]);
        }

    for (i = 0; i < Maxhosts; i++)
        for (j = 0; j < Maxhosts; j++) {
            if (j == 0)
                printf("\nREP[%02d][] = ", i);
            else if (j % 5)
                printf("%d  ", repports[i][j]);
            else
                printf("%d  \n            ", reqports[i][j]);
        }
#endif /* JIA_DEBUG */

    /***********Initialize commreq ********************/

    commreq.rcv_maxfd = 0;
    commreq.snd_maxfd = 0;
    FD_ZERO(&(commreq.snd_set));
    FD_ZERO(&(commreq.rcv_set));
    for (i = 0; i < Maxhosts; i++) {
        fd = req_fdcreate(i, 0); // create socket and bind it to [INADDR_ANY,
                                 // reqports[jia_pid][i]]
        commreq.rcv_fds[i] =
            fd; // request from (host i) is will be receive from
                // commreq.rcv_fds[i] (whose port = reqports[jia_pid][i]])
        FD_SET(fd, &commreq.rcv_set);
        commreq.rcv_maxfd = MAX(fd + 1, commreq.rcv_maxfd);

        if (0 > fcntl(commreq.rcv_fds[i], F_SETOWN,
                      getpid())) // set current process to receive SIGIO signal
            assert0(0, "initcomm()-->fcntl(..F_SETOWN..)");

        // if (0 > fcntl(commreq.rcv_fds[i], F_SETFL, FASYNC|FNDELAY))
        if (0 > fcntl(commreq.rcv_fds[i], F_SETFL, O_ASYNC | O_NONBLOCK))
            assert0(0, "initcomm()-->fcntl(..F_SETFL..)");

        fd = req_fdcreate(i, 1);
        commreq.snd_fds[i] = fd; // snd_fds socket fd with random port
        FD_SET(fd, &commreq.snd_set);
        commreq.snd_maxfd = MAX(fd + 1, commreq.snd_maxfd);
    }
    for (i = 0; i < Maxhosts; i++) {
        commreq.snd_seq[i] = 0;
        commreq.rcv_seq[i] = 0;
    }

    /***********Initialize commrep ********************/

    commrep.rcv_maxfd = 0;
    commrep.snd_maxfd = 0;
    FD_ZERO(&(commrep.snd_set));
    FD_ZERO(&(commrep.rcv_set));

    for (i = 0; i < Maxhosts; i++) {
        fd = rep_fdcreate(i, 0);
        commrep.rcv_fds[i] =
            fd; // reply from (host i) will be received from commrep.rcv_fds[i]
                // (ports = repports[jiapid][i])
        FD_SET(fd, &(commrep.rcv_set));
        commrep.rcv_maxfd = MAX(fd + 1, commrep.rcv_maxfd);

        fd = rep_fdcreate(i, 1); // fd with random port
        commrep.snd_fds[i] = fd; // the reply to (host i) will be sended to
                                 // snd_fds[i] (whose port is random)
        FD_SET(fd, &commrep.snd_set);
        commrep.snd_maxfd = MAX(fd + 1, commrep.snd_maxfd);
    }
}

/**
 * @brief msgserver -- according to inqueue head msg.op choose different server
 *
 */
void msgserver() {
    SPACE(1);
    printf("Enterserver msg[%d], incount=%d, inhead=%d, intail=%d!\n", inqh.op,
           incount, inhead, intail);
    switch (inqh.op) {
    case DIFF:
        diffserver(&inqh);
        break;
    case DIFFGRANT:
        diffgrantserver(&inqh);
        break;
    case GETP:
        getpserver(&inqh);
        break;
    case GETPGRANT:
        getpgrantserver(&inqh);
        break;
    case ACQ:
        acqserver(&inqh);
        break;
    case ACQGRANT:
        acqgrantserver(&inqh);
        break;
    case INVLD:
        invserver(&inqh);
        break;
    case BARR:
        barrserver(&inqh);
        break;
    case BARRGRANT:
        barrgrantserver(&inqh);
        break;
    case REL:
        relserver(&inqh);
        break;
    case WTNT:
        wtntserver(&inqh);
        break;
    case JIAEXIT:
        jiaexitserver(&inqh);
        break;
    case WAIT:
        waitserver(&inqh);
        break;
    case WAITGRANT:
        waitgrantserver(&inqh);
        break;

    case SETCV:
        setcvserver(&inqh);
        break;
    case RESETCV:
        resetcvserver(&inqh);
        break;
    case WAITCV:
        waitcvserver(&inqh);
        break;
    case CVGRANT:
        cvgrantserver(&inqh);
        break;
    case MSGBODY:
    case MSGTAIL:
        msgrecvserver(&inqh);
        break;
    case LOADREQ:
        loadserver(&inqh);
        break;
    case LOADGRANT:
        loadgrantserver(&inqh);
        break;
#ifdef DOSTAT
    case STAT:
        statserver(&inqh);
        break;
    case STATGRANT:
        statgrantserver(&inqh);
        break;
#endif

    default:
        if (inqh.op >= BCAST) {
            bcastserver(&inqh);
        } else {
            printmsg(&inqh, 1);
            assert0(0, "msgserver(): Incorrect Message!");
        }
        break;
    }
    SPACE(1);
    printf("Out servermsg!\n");
}

/**
 * @brief sigint_handler -- sigint handler
 *
 */
void sigint_handler() {
    assert(0, "Exit by user!!\n");
}

/*----------------------------------------------------------*/
#if defined SOLARIS || defined IRIX62
void sigio_handler(int sig, siginfo_t *sip, ucontext_t *uap)
#endif
#ifdef LINUX
    void sigio_handler()
#endif
#ifdef AIX41
        void sigio_handler()
#endif
{
    int res, len, oldindex;
    int i, s;
    fd_set readfds;
    struct sockaddr_in from, to;
    sigset_t set, oldset;
    int servemsg;
    int testresult;

#ifdef DOSTAT
    register unsigned int begin;
    if (statflag == 1) {
        jiastat.sigiocnt++;
        if (interruptflag == 0) {
            begin = get_usecs();
            if (jiastat.kernelflag == 0) {
                jiastat.usersigiocnt++;
            } else if (jiastat.kernelflag == 1) {
                jiastat.synsigiocnt++;
            } else if (jiastat.kernelflag == 2) {
                jiastat.segvsigiocnt++;
            }
        }
        interruptflag++;
    }
#endif

    printf("\nEnter sigio_handler!\n");

    servemsg = 0;
    readfds = commreq.rcv_set;
    polltime.tv_sec = 0;
    polltime.tv_usec = 0;
    // polltime equals 0, select will return immediately
    res = select(commreq.rcv_maxfd, &readfds, NULL, NULL,
                 &polltime); // whether there is a requested from other hosts
    while (res > 0) {
        // handle ready fd(from other hosts)
        for (i = 0; i < hostc; i++) {
            if (i != jia_pid)
                if (FD_ISSET(commreq.rcv_fds[i],
                             &readfds)) { // caught a request from host i
                    assert0((incount < Maxqueue),
                            "sigio_handler(): Inqueue exceeded!");

                    s = sizeof(from);
                    // receive data from host i and store it into
                    // inqueue[intail],
                    res = recvfrom(commreq.rcv_fds[i], (char *)&(inqt),
                                   Maxmsgsize + Msgheadsize, 0,
                                   (struct sockaddr *)&from, &s);
                    assert0((res >= 0), "sigio_handler()-->recvfrom()");

                    to.sin_family = AF_INET;
                    memcpy(&to.sin_addr, hosts[inqt.frompid].addr,
                           hosts[inqt.frompid].addrlen);
                    to.sin_port = htons(repports[inqt.frompid][inqt.topid]);

                    // send ack(actually seqno(4bytes)) to host i
                    res = sendto(commrep.snd_fds[i], (char *)&(inqt.seqno),
                                 sizeof(inqt.seqno), 0, (struct sockaddr *)&to,
                                 sizeof(to));
                    assert0((res != -1), "sigio_handler()-->sendto() ACK");

                    // new msg's seqno is greater than the former's from the
                    // same host
                    if (inqt.seqno > commreq.rcv_seq[i]) {
#ifdef DOSTAT
                        if (statflag == 1) {
                            if (inqt.frompid != inqt.topid) {
                                jiastat.msgrcvcnt++;
                                jiastat.msgrcvbytes +=
                                    (inqt.size + Msgheadsize);
                            }
                        }
#endif
                
                        BEGINCS; // modify global variables (mask io signal)
                        servemsg = inqrecv(i);
                        ENDCS;
                    } else {
                        printmsg(&inqt, 1);
                        printf("Receive resend message!\n");
#ifdef DOSTAT
                        jiastat.resentcnt++;
#endif
                    }
                }
        }
        // check whether there are more data or not
        readfds = commreq.rcv_set;
        polltime.tv_sec = 0;
        polltime.tv_usec = 0;
        res = select(commreq.rcv_maxfd, &readfds, NULL, NULL, &polltime);
    }

    SPACE(1);
    printf("Finishrecvmsg!inc=%d,inh=%d,int=%d\n", incount, inhead, intail);
    // handle msg
    while (servemsg == 1) {
        msgserver();
        BEGINCS;
        servemsg = inqcomp();
        ENDCS;
    }

    SPACE(1);
    printf("Out sigio_handler!\n");
#ifdef DOSTAT
    if (statflag == 1) {
        interruptflag--;
        if (interruptflag == 0) {
            if (jiastat.kernelflag == 0) {
                jiastat.usersigiotime += get_usecs() - begin;
            } else if (jiastat.kernelflag == 1) {
                jiastat.synsigiotime += get_usecs() - begin;
            } else if (jiastat.kernelflag == 2) {
                jiastat.segvsigiotime += get_usecs() - begin;
            }
        }
    }
#endif // DOSTAT
}

/**
 * @brief asendmsg() -- send msg to outqueue[outtail], and call outsend()
 *
 * @param msg
 */
void asendmsg(jia_msg_t *msg) {
    int outsendmsg;
#ifdef DOSTAT
    register unsigned int begin = get_usecs();
    if (statflag == 1) {
        if (msg->size > 4096)
            jiastat.largecnt++;
        if (msg->size < 128)
            jiastat.smallcnt++;
    }
#endif

    printf("Enter asendmsg!");

    // printmsg(msg, 1);

    BEGINCS;
    assert0((outcount < Maxqueue), "asendmsg(): Outqueue exceeded!");
    memcpy(&(outqt), msg,
           Msgheadsize + msg->size); // copy msg to outqueue[outtail]
    commreq.snd_seq[msg->topid]++;
    outqt.seqno = commreq.snd_seq[msg->topid];
    outcount++;
    outtail = (outtail + 1) % Maxqueue;
    outsendmsg = (outcount == 1) ? 1 : 0;
    ENDCS;
    printf("Before outsend(), Out asendmsg! outc=%d, outh=%d, outt=%d\n",
           outcount, outhead, outtail);
    while (outsendmsg == 1) { // there is msg need to be sended in outqueue
        outsend();
        BEGINCS;
        outhead = (outhead + 1) % Maxqueue;
        outcount--;
        outsendmsg = (outcount > 0) ? 1 : 0;
        ENDCS;
    }
    printf("Out asendmsg! outc=%d, outh=%d, outt=%d\n", outcount, outhead,
           outtail);

#ifdef DOSTAT
    if (statflag == 1) {
        jiastat.asendtime += get_usecs() - begin;
    }
#endif
}

/**
 * @brief outsend -- outsend the outqueue[outhead] msg
 *
 */
void outsend() {
    int res, toproc, fromproc;
    struct sockaddr_in to, from;
    int rep;
    int retries_num;
    unsigned long start, end;
    int msgsize;
    int s;
    int sendsuccess, arrived;
    fd_set readfds;
    int servemsg;
#ifdef DOSTAT
    register unsigned int begin;
#endif

    VERBOSE_OUT(1, "\nEnter outsend!\n");

    printmsg(&outqh, 1);

    toproc = outqh.topid;
    fromproc = outqh.frompid;
    VERBOSE_OUT(1, "outc=%d, outh=%d, outt=%d\n \
                outqueue[outhead].topid = %d outqueue[outhead].frompid = %d\n",
                outcount, outhead, outtail, toproc, fromproc);

    if (toproc == fromproc) { // single machine communication

        // recv msg (outq to inq)
        BEGINCS;
        memcpy(&(inqt), &(outqh), Msgheadsize + outqh.size);
        servemsg = inqrecv(fromproc);
        ENDCS;

        VERBOSE_OUT(
            1,
            "Finishcopymsg,incount=%d,inhead=%d,intail=%d!\nservemsg == %d\n",
            incount, inhead, intail, servemsg);

        while (servemsg == 1) { // there are some msg need be served
            msgserver();
            BEGINCS;
            servemsg = inqcomp();
            ENDCS;
        }
    } else { // comm between different hosts
        msgsize = outqh.size + Msgheadsize;
#ifdef DOSTAT
        if (statflag == 1) {
            jiastat.msgsndcnt++;
            jiastat.msgsndbytes += msgsize;
        }
#endif
        to.sin_family = AF_INET;
        printf("toproc IP address is %s, addrlen is %d\n",
               inet_ntoa(*(struct in_addr *)hosts[toproc].addr),
               hosts[toproc].addrlen);
        memcpy(&to.sin_addr, hosts[toproc].addr, hosts[toproc].addrlen);
        to.sin_port = htons(reqports[toproc][fromproc]);

        printf("reqports[toproc][fromproc] = %u\n", reqports[toproc][fromproc]);

        retries_num = 0;
        sendsuccess = 0;

        printf("commreq.snd_fds[toproc] = %d\n", commreq.snd_fds[toproc]);
        printf("commreq.rcv_fds[toproc] = %d\n", commreq.rcv_fds[toproc]);
        while ((retries_num < MAX_RETRIES) &&
               (sendsuccess != 1)) { // retransimission
            BEGINCS;
            res = sendto(commreq.snd_fds[toproc], (char *)&(outqh), msgsize, 0,
                         (struct sockaddr *)&to, sizeof(to));
            assert0((res != -1), "outsend()-->sendto()");
            ENDCS;

            arrived = 0;
            start = jia_current_time();
            end = start + TIMEOUT;

            while ((jia_current_time() < end) &&
                   (arrived != 1)) { // wait for ack
                FD_ZERO(&readfds);
                FD_SET(commrep.rcv_fds[toproc], &readfds);
                polltime.tv_sec = 0;
                polltime.tv_usec = 0;
                res =
                    select(commrep.rcv_maxfd, &readfds, NULL, NULL, &polltime);
                if (FD_ISSET(commrep.rcv_fds[toproc], &readfds) != 0) {
                    arrived = 1;
                }
            }
            printf("arrived = %d\n", arrived);
            if (arrived == 1) {
            recv_again:
                s = sizeof(from);
                BEGINCS;
                res = recvfrom(commrep.rcv_fds[toproc], (char *)&rep, Intbytes,
                               0, (struct sockaddr *)&from, &s);
                ENDCS;
                if ((res < 0) && (errno == EINTR)) {
                    printf("A signal interrupted recvfrom() before any data "
                           "was available\n");
                    goto recv_again;
                }
                if ((res != -1) && (rep == outqh.seqno)) {
                    sendsuccess = 1;
                }
            }
            retries_num++;
        }

        if (sendsuccess != 1) {
            printf("I am host %d, hostname = %s, I am running outsend() "
                   "function\n",
                   jia_pid, hosts[jia_pid].name);
            sprintf(errstr, "I Can't asend message(%d,%d) to host %d!",
                    outqh.op, outqh.seqno, toproc);
            printf("BUFFER SIZE %d (%d)\n", outqh.size, msgsize);
            assert0((sendsuccess == 1), errstr);
        }
    }

    printf("Out outsend!\n\n");
}

/**
 * @brief bsendmsg -- broadcast msg
 *
 * @param msg
 */
void bsendmsg(jia_msg_t *msg) {
    unsigned int root, level;

    msg->op += BCAST; // op >= BCAST, always call bcastserver

    root = jia_pid; // current host as root

    if (hostc == 1) { // single machine
        level = 1;
    } else {
        for (level = 0; (1 << level) < hostc; level++)
            ; // TODO: some problem here (hostc==2, level equals 1 too)
    }

    msg->temp = ((root & 0xffff) << 16) | (level & 0xffff);

    bcastserver(msg);
}

/**
 * @brief bcastserver --
 *
 * @param msg
 */
void bcastserver(jia_msg_t *msg) {
    int mypid, child1, child2;
    int rootlevel, root, level;

    rootlevel = msg->temp;
    root = (rootlevel >> 16) & 0xffff;
    level = rootlevel & 0xffff;
    level--;

    mypid =
        ((jia_pid - root) >= 0) ? (jia_pid - root) : (jia_pid - root + hostc);
    child1 = mypid;
    child2 = mypid + (1 << level);

    if (level == 0) {
        msg->op -= BCAST;
    }
    msg->temp = ((root & 0xffff) << 16) | (level & 0xffff);
    msg->frompid = jia_pid;

    if (child2 < hostc) {
        msg->topid = (child2 + root) % hostc;
        asendmsg(msg);
    }

    msg->topid = (child1 + root) % hostc;
    asendmsg(msg);
}

#else  /* NULL_LIB */
#endif /* NULL_LIB */
