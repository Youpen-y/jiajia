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

#ifndef NULL_LIB
#include "utils.h"
#include "comm.h"       // statgrantserver, 
#include "mem.h"        // diffserver, getserver, diffgrantserver, getpgrantserver
#include "tools.h"      // jiaexitserver, enable_sigio, disable_sigio, jia_current_time, newmsg, printmsg, get_usecs, emptyprintf
#include "setting.h"
#include "syn.h"        // acqserver, inverver, relserver, wtntserver, barrserver, barrgrantserver, acqgrantserver
                        // waitgrantserver, waitserver, setcvserver, resetcvserver, waitcvserver, cvgrantserver,
#include "load.h"       // loadserver, loadgrantserver,
#include "msg.h"        // msgrecvserver
#include "stat.h"       // statserver

#define BEGINCS                                                                \
    {                                                                          \
        sigset_t newmask, oldmask;                                             \
        sigemptyset(&newmask);                                                 \
        sigaddset(&newmask, SIGIO);                                            \
        sigprocmask(SIG_BLOCK, &newmask, &oldmask);                            \
        oldsigiomask = sigismember(&oldmask, SIGIO);                           \
        VERBOSE_LOG(3, "Enter CS\t");                                          \
    }
#define ENDCS                                                                  \
    {                                                                          \
        if (oldsigiomask == 0)                                                 \
            enable_sigio();                                                    \
        VERBOSE_LOG(3, "Exit CS\n");                                           \
    }

// #ifndef JIA_DEBUG
// #define  msgprint  0
// #define  VERBOSE_LOG 3,   emptyprintf
// #else  /* JIA_DEBUG */
// #define msgprint  1
// #endif  /* JIA_DEBUG */
// #define msgprint 1

msg_buffer_t msg_buffer = {0};

// global variables
/* request/reply communication manager*/
// CommManager commreq, commrep;
comm_manager_t req_manager;
comm_manager_t rep_manager;

msg_queue_t inqueue;
msg_queue_t outqueue;

// /* head msg, tail msg, msg count in out queue */
// volatile int inhead, intail, incount;
// volatile int outhead, outtail, outcount;

/* commreq used to send and recv request, commrep used to send and recv reply */
// unsigned short reqports[Maxhosts][Maxhosts]; // every host has Maxhosts request ports
// unsigned short repports[Maxhosts][Maxhosts]; // every host has Maxhosts reply ports

unsigned short start_port;
int oldsigiomask;


// function definitions

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

    if (system_setting.jia_pid == 0) {
        VERBOSE_LOG(3, "************Initialize Communication!*******\n");
    }
    VERBOSE_LOG(3, "current jia_pid = %d\n", system_setting.jia_pid);
    VERBOSE_LOG(3, " start_port = %u \n", start_port);

    init_msg_buffer();  // initialize msg array and corresponding flag that indicate busy or free

    init_queue(&inqueue, system_setting.inqueue_size);  // init input msg queue
    init_queue(&outqueue, system_setting.outqueue_size); // init output msg queue

#if defined SOLARIS || defined IRIX62
    {
        struct sigaction act;

        act.sa_handler = (void_func_handler)sigio_handler;
        sigemptyset(&act.sa_mask);
        act.sa_flags = SA_NODEFER | SA_SIGINFO;
        if (sigaction(SIGIO, &act, NULL))
            local_assert(0, "initcomm()-->sigaction()");

        act.sa_handler = (void_func_handler)sigint_handler;
        sigemptyset(&act.sa_mask);
        act.sa_flags = SA_SIGINFO;
        if (sigaction(SIGINT, &act, NULL)) {
            local_assert(0, "segv sigaction problem");
        }
    }
#endif
#ifdef LINUX
    {
        struct sigaction act;

        // sigio's action: sigio_handler
        act.sa_handler = (void_func_handler)sigio_handler;
        sigemptyset(&act.sa_mask);
        act.sa_flags = SA_NODEFER | SA_RESTART;
        if (sigaction(SIGIO, &act, NULL))
            local_assert(0, "initcomm()-->sigaction()");

        // sigint's action: sigint_handler
        act.sa_handler = (void_func_handler)sigint_handler;
        sigemptyset(&act.sa_mask);
        act.sa_flags = SA_NODEFER;
        if (sigaction(SIGINT, &act, NULL)) {
            local_assert(0, "segv sigaction problem");
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



#ifdef JIA_DEBUG
    for (i = 0; i < Maxhosts; i++)
        for (j = 0; j < Maxhosts; j++) {
            if (j == 0)
                VERBOSE_LOG(3, "\nREQ[%02d][] = ", i);
            else if (j % 5)
                VERBOSE_LOG(3, "%d  ", reqports[i][j]);
            else
                VERBOSE_LOG(3, "%d  \n            ", reqports[i][j]);
        }

    for (i = 0; i < Maxhosts; i++)
        for (j = 0; j < Maxhosts; j++) {
            if (j == 0)
                VERBOSE_LOG(3, "\nREP[%02d][] = ", i);
            else if (j % 5)
                VERBOSE_LOG(3, "%d  ", repports[i][j]);
            else
                VERBOSE_LOG(3, "%d  \n            ", reqports[i][j]);
        }
#endif /* JIA_DEBUG */

    /***********Initialize commreq ********************/

    // commreq.rcv_maxfd = 0;
    // commreq.snd_maxfd = 0;
    // FD_ZERO(&(commreq.snd_set));
    // FD_ZERO(&(commreq.rcv_set));
    for (i = 0; i < Maxhosts; i++) {
        fd = req_fdcreate(i, 0); // create socket and bind it to [INADDR_ANY,
                                 // reqports[jia_pid][i]]
        commreq.rcv_fds[i] =
            fd; // request from (host i) is will be receive from
                // commreq.rcv_fds[i] (whose port = reqports[jia_pid][i]])
        // FD_SET(fd, &commreq.rcv_set);
        // commreq.rcv_maxfd = MAX(fd + 1, commreq.rcv_maxfd);

        if (0 > fcntl(commreq.rcv_fds[i], F_SETOWN,
                      getpid())) // set current process to receive SIGIO signal
            local_assert(0, "initcomm()-->fcntl(..F_SETOWN..)");

        // if (0 > fcntl(commreq.rcv_fds[i], F_SETFL, FASYNC|FNDELAY))
        if (0 > fcntl(commreq.rcv_fds[i], F_SETFL, O_ASYNC | O_NONBLOCK))
            local_assert(0, "initcomm()-->fcntl(..F_SETFL..)");

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

    pthread_create(&client_tid, NULL, client_thread, NULL);   // create a new thread to listen to commreq
    pthread_create(&server_tid, NULL, server_thread, NULL);   // create a new thread to listen to commrep


}

void init_msg_buffer(){
    msg_buffer.size     = system_setting.msg_buffer_size;
    msg_buffer.msgarray = (jia_msg_t*)malloc(sizeof(jia_msg_t) * msg_buffer.size);
    msg_buffer.msgbusy  = (int*)malloc(sizeof(int) * msg_buffer.size);

    for(int i = 0; i < msg_buffer.size; i++){
        msg_buffer.msgarray[i].index = i;
        msg_buffer.msgbusy[i] = 0;
    }
}

void free_msg_buffer(){
    free(msg_buffer.msgarray);
    free(msg_buffer.msgbusy);
}

/**
 * @brief inqrecv() -- recv msg (update incount&&intail)
 *
 * @return iff incount==1
 */
static inline int inqrecv(int fromproc) {
    printmsg(&inqt, 1);
    local_assert((incount < Maxqueue), "outsend(): Inqueue exceeded!");
    incount++;
    intail = (intail + 1) % Maxqueue;
    // update seqno from host fromproc
    commreq.rcv_seq[fromproc] = inqt.seqno;
    VERBOSE_LOG(3, "incount: %d\n", incount);
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
    VERBOSE_LOG(3, "incount: %d\n", incount);
    return (incount > 0);
};

/**
 * @brief inqrecv() -- send msg (update outcount&&outtail)
 *
 * @return iff outcount==1
 */
static inline int outqsend(int toproc) {
    local_assert((outcount < Maxqueue), "asendmsg(): Outqueue exceeded!");
    commreq.snd_seq[toproc]++;
    outqt.seqno = commreq.snd_seq[toproc];
    outcount++;
    outtail = (outtail + 1) % Maxqueue;
    return (outcount == 1);
};

/**
 * @brief outqcomp() -- complete msg (update outcount&&outhead)
 *
 * @return iff outcount > 0
 */
static inline int outqcomp() {
    outhead = (outhead + 1) % Maxqueue;
    outcount--;
    return (outcount > 0);
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
    local_assert((fd != -1), "req_fdcreate()-->socket()");

#ifdef SOLARIS
    size = Maxmsgsize + Msgheadsize + 128;
    res = setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (char *)&size, sizeof(size));
    local_assert((res == 0), "req_fdcreate()-->setsockopt():SO_RCVBUF");

    size = Maxmsgsize + Msgheadsize + 128;
    res = setsockopt(fd, SOL_SOCKET, SO_SNDBUF, (char *)&size, sizeof(size));
    local_assert((res == 0), "req_fdcreate()-->setsockopt():SO_SNDBUF");
#endif
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = (flag) ? htons(0) : htons(reqports[system_setting.jia_pid][i]);

    res = bind(fd, (struct sockaddr *)&addr, sizeof(addr));
    local_assert((res == 0), "req_fdcreate()-->bind()");

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
    local_assert((fd != -1), "rep_fdcreate()-->socket()");

#ifdef SOLARIS
    size = Intbytes;
    res = setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (char *)&size, sizeof(size));
    local_assert((res == 0), "rep_fdcreate()-->setsockopt():SO_RCVBUF");

    size = Intbytes;
    res = setsockopt(fd, SOL_SOCKET, SO_SNDBUF, (char *)&size, sizeof(size));
    local_assert((res == 0), "rep_fdcreate()-->setsockopt():SO_SNDBUF");
#endif /* SOLARIS */

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = (flag) ? htons(0) : htons(repports[system_setting.jia_pid][i]);

    res = bind(fd, (struct sockaddr *)&addr, sizeof(addr));
    local_assert((res == 0), "rep_fdcreate()-->bind()");
    return fd;
}


/**
 * @brief msgserver -- according to inqueue head msg.op choose different server
 *
 */
void msgserver() {
    SPACE(1);
    VERBOSE_LOG(3, "Enterserver msg[%d], incount=%d, inhead=%d, intail=%d!\n",
                inqh.op, incount, inhead, intail);
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
    case STAT:
        statserver(&inqh);
        break;
    case STATGRANT:
        statgrantserver(&inqh);
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

    default:
        if (inqh.op >= BCAST) {
            bcastserver(&inqh);
        } else {
            printmsg(&inqh, 1);
            local_assert(0, "msgserver(): Incorrect Message!");
        }
        break;
    }
    SPACE(1);
    VERBOSE_LOG(3, "Out servermsg!\n");
}

/**
 * @brief sigint_handler -- sigint handler
 *
 */
void sigint_handler() {
    jia_assert(0, "Exit by user!!\n");
}

/*----------------------------------------------------------*/
#if defined SOLARIS || defined IRIX62
void sigio_handler(int sig, siginfo_t *sip, ucontext_t *uap)
#endif

/**
 * @brief sigio_handler -- sigio handler (it can be interrupt)
 *
 */

#if defined LINUX || defined AIX41
    void sigio_handler()
#endif
{
    int res, len, oldindex;
    int i, s;
    fd_set readfds;
    struct sockaddr_in from, to;
    sigset_t set, oldset;
    int servemsg = 0;
    struct timeval zerotime = {0, 0};

    // begin segvio time
#ifdef DOSTAT
    register unsigned int begin;
    STATOP(jiastat.sigiocnt++; if (interruptflag == 0) {
        begin = get_usecs();
        if (jiastat.kernelflag == 0) {
            jiastat.usersigiocnt++;
        } else if (jiastat.kernelflag == 1) {
            jiastat.synsigiocnt++;
        } else if (jiastat.kernelflag == 2) {
            jiastat.segvsigiocnt++;
        }
    } interruptflag++;)
#endif

    VERBOSE_LOG(3, "\nEnter sigio_handler!\n");

    // whether there is a requested from other hosts
    readfds = commreq.rcv_set;
    res = select(commreq.rcv_maxfd, &readfds, NULL, NULL, &zerotime);
    while (res > 0) {
        // handle ready fd(from other hosts)
        for (i = 0; i < system_setting.hostc; i++) {
            if ((i != system_setting.jia_pid) && FD_ISSET(commreq.rcv_fds[i], &readfds)) {

                /* step 1: receive data from host i and store it into
                 * inqueue[intail]*/
                s = sizeof(from);
                res = recvfrom(commreq.rcv_fds[i], (char *)&(inqt),
                               Maxmsgsize + Msgheadsize, 0,
                               (struct sockaddr *)&from, &s);
                local_assert((res >= 0), "sigio_handler()-->recvfrom()");

                /* step 2: init socket to && send ack(actually
                 * seqno(4bytes)) to host i */
                /* must send ack before inqrecv update intail(ack use intail)
                 */
                to.sin_family = AF_INET;
                to.sin_addr.s_addr = inet_addr(system_setting.hosts[i].ip);
                to.sin_port = htons(repports[inqt.frompid][inqt.topid]);
                res = sendto(commrep.snd_fds[i], (char *)&(inqt.seqno),
                             sizeof(inqt.seqno), 0, (struct sockaddr *)&to,
                             sizeof(to));
                local_assert((res != -1), "sigio_handler()-->sendto() ACK");

                /* step 3: recv msg iff new msg's seqno is greater than the
                   former's from the same host, instead print resend  */
                if (inqt.seqno > commreq.rcv_seq[i]) {
#ifdef DOSTAT
                    STATOP(if (inqt.frompid != inqt.topid) {
                        jiastat.msgrcvcnt++;
                        jiastat.msgrcvbytes += (inqt.size + Msgheadsize);
                    })
#endif

                    BEGINCS;
                    servemsg = inqrecv(i);
                    ENDCS;
                } else {
                    printmsg(&inqt, 1);
                    VERBOSE_LOG(3, "Receive resend message!\n");
#ifdef DOSTAT
                    STATOP(jiastat.resentcnt++;)
#endif
                }
            }
        }
        // check whether there are more data or not
        readfds = commreq.rcv_set;
        res = select(commreq.rcv_maxfd, &readfds, NULL, NULL, &zerotime);
    }

    SPACE(1);
    VERBOSE_LOG(3, "Finishrecvmsg!inc=%d,inh=%d,int=%d\n", incount, inhead,
                intail);

    // handle msg
    while (servemsg == 1) {
        msgserver();
        BEGINCS;
        servemsg = inqcomp();
        ENDCS;
    }

    SPACE(1);
    VERBOSE_LOG(3, "Out sigio_handler!\n");

    // end segvio time
#ifdef DOSTAT
    STATOP(interruptflag--; if (interruptflag == 0) {
        if (jiastat.kernelflag == 0) {
            jiastat.usersigiotime += get_usecs() - begin;
        } else if (jiastat.kernelflag == 1) {
            jiastat.synsigiotime += get_usecs() - begin;
        } else if (jiastat.kernelflag == 2) {
            jiastat.segvsigiotime += get_usecs() - begin;
        }
    })
#endif
}

/**
 * @brief asendmsg() -- send msg to outqueue[outtail], and call outsend()
 *
 * @param msg
 */
void asendmsg(jia_msg_t *msg) {

    int outsendmsg = 0;

    // begin asend time && cal s/l jiamsg cnt
#ifdef DOSTAT
    register unsigned int begin = get_usecs();
    STATOP(if (msg->size > 4096) jiastat.largecnt++;
           if (msg->size < 128) jiastat.smallcnt++;)
#endif

    VERBOSE_LOG(3, "Enter asendmsg!");

    printmsg(msg, 1);

    /* step 1: memcpy to outqt && update outqueue */
    BEGINCS;
    memcpy(&(outqt), msg, Msgheadsize + msg->size);
    outsendmsg = outqsend(outqt.topid);
    ENDCS;
    VERBOSE_LOG(3,
                "Before outsend(), Out asendmsg! outc=%d, outh=%d, outt=%d\n",
                outcount, outhead, outtail);

    /* step 2: call outsend() to send msg && update outqueue */
    // there is msg need to be sent in outqueue
    while (outsendmsg == 1) {
        outsend();
        BEGINCS;
        outsendmsg = outqcomp();
        ENDCS;
    }
    VERBOSE_LOG(3, "Out asendmsg! outc=%d, outh=%d, outt=%d\n", outcount,
                outhead, outtail);

    // end asend time
#ifdef DOSTAT
    STATOP(jiastat.asendtime += get_usecs() - begin;)
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
    struct timeval zerotime = {0, 0};
#ifdef DOSTAT
    register unsigned int begin;
#endif

    VERBOSE_LOG(3, "\nEnter outsend!\n");

    printmsg(&outqh, 1);

    toproc = outqh.topid;
    fromproc = outqh.frompid;
    VERBOSE_LOG(3, "outc=%d, outh=%d, outt=%d\n \
                outqueue[outhead].topid = %d outqueue[outhead].frompid = %d\n",
                outcount, outhead, outtail, toproc, fromproc);

    if (toproc == fromproc) { // single machine communication

        /* step 1: memcpy to inqt && update inqueue */
        BEGINCS;
        memcpy(&(inqt), &(outqh), Msgheadsize + outqh.size);
        servemsg = inqrecv(fromproc);
        ENDCS;

        VERBOSE_LOG(
            3,
            "Finishcopymsg,incount=%d,inhead=%d,intail=%d!\nservemsg == %d\n",
            incount, inhead, intail, servemsg);

        /* step 2: call msgserver() to manage msg && update inqueue */
        // there are some msg need be served
        while (servemsg == 1) {
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
        VERBOSE_LOG(3, "toproc IP address is %s\n", system_setting.hosts[toproc].ip);
        to.sin_addr.s_addr = inet_addr(system_setting.hosts[toproc].ip);
        to.sin_port = htons(reqports[toproc][fromproc]);

        VERBOSE_LOG(3, "reqports[toproc][fromproc] = %u\n",
                    reqports[toproc][fromproc]);

        retries_num = 0;
        sendsuccess = 0;

        VERBOSE_LOG(3, "commreq.snd_fds[toproc] = %d\n",
                    commreq.snd_fds[toproc]);
        VERBOSE_LOG(3, "commreq.rcv_fds[toproc] = %d\n",
                    commreq.rcv_fds[toproc]);
        while ((retries_num < MAX_RETRIES) &&
               (sendsuccess != 1)) { // retransimission
            BEGINCS;
            res = sendto(commreq.snd_fds[toproc], (char *)&(outqh), msgsize, 0,
                         (struct sockaddr *)&to, sizeof(to));
            local_assert((res != -1), "outsend()-->sendto()");
            ENDCS;

            arrived = 0;
            start = jia_current_time();
            end = start + TIMEOUT;

            while ((jia_current_time() < end) &&
                   (arrived != 1)) { // wait for ack
                FD_ZERO(&readfds);
                FD_SET(commrep.rcv_fds[toproc], &readfds);
                res =
                    select(commrep.rcv_maxfd, &readfds, NULL, NULL, &zerotime);
                arrived = (FD_ISSET(commrep.rcv_fds[toproc], &readfds) != 0);
            }
            VERBOSE_LOG(3, "arrived = %d\n", arrived);
            if (arrived) {
            recv_again:
                s = sizeof(from);
                BEGINCS;
                res = recvfrom(commrep.rcv_fds[toproc], (char *)&rep, Intbytes,
                               0, (struct sockaddr *)&from, &s);
                ENDCS;
                if ((res < 0) && (errno == EINTR)) {
                    VERBOSE_LOG(
                        3, "A signal interrupted recvfrom() before any data "
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
            VERBOSE_LOG(3,
                        "I am host %d, hostname = %s, I am running outsend() "
                        "function\n",
                        system_setting.jia_pid, system_setting.hosts[system_setting.jia_pid].username);
            sprintf(errstr, "I Can't asend message(%d,%d) to host %d!",
                    outqh.op, outqh.seqno, toproc);
            VERBOSE_LOG(3, "BUFFER SIZE %d (%d)\n", outqh.size, msgsize);
            local_assert((sendsuccess == 1), errstr);
        }
    }

    VERBOSE_LOG(3, "Out outsend!\n\n");
}

/**
 * @brief bsendmsg -- broadcast msg
 *
 * @param msg
 */
void bsendmsg(jia_msg_t *msg) {
    unsigned int root, level;

    msg->op += BCAST; // op >= BCAST, always call bcastserver

    root = system_setting.jia_pid; // current host as root

    if (system_setting.hostc == 1) { // single machine
        level = 1;
    } else {
        for (level = 0; (1 << level) < system_setting.hostc; level++)
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

    int jia_pid = system_setting.jia_pid;
    int hostc = system_setting.hostc;

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



int init_queue(msg_queue_t *msg_queue, int size)
{
    msg_queue->queue = (msg_queue_slot_t *)malloc(sizeof(msg_queue_slot_t) * size);
    msg_queue->head = 0;
    msg_queue->tail = 0;
    msg_queue->size = size;
    sem_init(&msg_queue->busy_count, 0, 0);
    sem_init(&msg_queue->free_count, 0, size);
    return 0;
}

int enqueue(msg_queue_t *queue, jia_msg_t *msg)
{
    sem_wait(&queue->free_count);
    memcpy(&(queue->queue[queue->tail]), msg, sizeof(jia_msg_t));
    queue->tail = (queue->tail + 1) % queue->size;
    sem_post(&queue->busy_count);
    return 0;
}

jia_msg_t *dequeue(msg_queue_t *msg_queue)
{
    sem_wait(&msg_queue->busy_count);
    jia_msg_t *msg = &(msg_queue->queue[queue->head]);
    msg_queue->head = (msg_queue->head + 1) % msg_queue->size;
    sem_post(&msg_queue->free_count);
    return msg;
}

void free_queue(msg_queue_t *msg_queue)
{
    free(msg_queue->queue);
}


int init_comm_manager(){
    for (int i = 0; i < Maxhosts; i++) {
       for (int j = 0; j < Maxhosts; j++) {
            req_manager.snd_ports
            reqports[i][j] = start_port + i * Maxhosts + j;
            repports[i][j] = start_port + Maxhosts * Maxhosts + i * Maxhosts + j;
        }
    } 

}

#else  /* NULL_LIB */
#endif /* NULL_LIB */
