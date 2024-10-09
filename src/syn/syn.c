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
#include "syn.h"
#include "comm.h"
#include "global.h"
#include "init.h"
#include "jia.h"
#include "mem.h"


extern void assert(int cond, char *errstr);
extern jia_msg_t *newmsg();
extern void freemsg(jia_msg_t *);
extern void printmsg(jia_msg_t *msg, int right);
extern void asendmsg(jia_msg_t *req);
extern void savediff(int cachei);
extern wtnt_t *newwtnt();
extern void freewtntspace(wtnt_t *ptr);
extern void freetwin(address_t *twin);
extern void memprotect(void *, size_t, int);
extern float jia_clock();
extern void setwtvect(int homei, wtvect_t wv);
extern void addwtvect(int homei, wtvect_t wv, int from);

extern unsigned int get_usecs();
extern void newtwin(address_t *twin);
extern void appendmsg(jia_msg_t *msg, unsigned char *str, int len);

extern void senddiffs();

extern void bsendmsg(jia_msg_t *msg);

extern void flushpage(int cachei);
extern void memunmap(void *addr, size_t len);

void initsyn();
void clearlocks();
void savewtnt(wtnt_t *ptr, address_t addr, int frompid);
void savepage(int cachei);
void endinterval(int);
void startinterval(int);
void acquire(int lock);
void pushstack(int lock);
void jia_lock(int lock);
wtnt_t *appendlockwtnts(jia_msg_t *msg, wtnt_t *ptr, int acqscope);
wtnt_t *appendstackwtnts(jia_msg_t *msg, wtnt_t *ptr);
wtnt_t *appendbarrwtnts(jia_msg_t *msg, wtnt_t *ptr);
void sendwtnts(int operation);
void popstack();
void jia_unlock(int lock);
void jia_barrier();
void grantlock(int lock, int toproc, int acqscope);
void grantbarr(int lock);
void acqserver(jia_msg_t *req);
void relserver(jia_msg_t *req);
void barrserver(jia_msg_t *req);
void recordwtnts(jia_msg_t *req);
void wtntserver(jia_msg_t *req);
void invalidate(jia_msg_t *req);
void invserver(jia_msg_t *req);
void barrgrantserver(jia_msg_t *req);
void acqgrantserver(jia_msg_t *req);
void broadcast(jia_msg_t *msg);
void jia_setcv(int cvnum);
void jia_resetcv(int cvnum);
void jia_waitcv(int cvnum);
void jia_wait();
void waitserver(jia_msg_t *req);
void waitgrantserver(jia_msg_t *req);
void grantcondv(int condv, int toproc);
void setcvserver(jia_msg_t *req);
void resetcvserver(jia_msg_t *req);
void waitcvserver(jia_msg_t *req);
void cvgrantserver(jia_msg_t *req);
void migarrangehome();
void migcheckcache();
void migpage(unsigned long, int, int);

extern int jia_pid;
extern host_t hosts[Maxhosts];
extern int hostc;
extern jiapage_t page[Maxmempages + 1];
extern jiacache_t cache[Cachepages + 1];
extern jiahome_t home[Homepages];
extern char errstr[Linesize];
extern volatile int diffwait;
extern int H_MIG, AD_WD, B_CAST, LOAD_BAL, W_VEC;
extern float caltime, starttime, endtime;

#ifdef DOSTAT
extern int statflag;
extern jiastat_t jiastat;
#endif

jialock_t
    locks[Maxlocks + 1]; // lock array, according to the hosts allocate lock(eg.
                         // host0: 0, 2,... 62; host1 = 1, 3, ..., 63)
jiastack_t lockstack[Maxstacksize]; // lock stack

int stackptr;
volatile int acqwait;
volatile int barrwait, noclearlocks;

volatile int waitwait, waitcounter;
volatile int cvwait;
jiacv_t condvars[Maxcvs];

/**
 * @brief initsyn -- initialize the sync setting
 *
 */
void initsyn() {
    int i, j, k;

    for (i = 0; i <= Maxlocks; i++) { // initialize locks setting
        locks[i].acqc = 0;
        locks[i].scope = 0;
        locks[i].myscope = -1;
        for (j = 0; j < Maxhosts; j++) {
            locks[i].acqs[j] = -1;
            locks[i].acqscope[j] = -1;
        }
        if ((i % hostc) == jia_pid)
            locks[i].wtntp = newwtnt();
        else
            locks[i].wtntp = WNULL;
    }

    for (i = 0; i < Maxstacksize; i++) {
        lockstack[i].lockid = 0;
        lockstack[i].wtntp = newwtnt();
    }

    stackptr = 0;
    top.lockid = hidelock;
    noclearlocks = 0;

    for (i = 0; i < Maxcvs; i++) {
        condvars[i].waitc = 0;
        for (j = 0; j < Maxhosts; j++) {
            condvars[i].waits[j] = -1;
        }
    }
    cvwait = 0;
    waitcounter = 0;
}

/**
 * @brief clearlocks() -- free the write notice space that used by lock
 *
 */
void clearlocks() {
    int i;

    for (i = jia_pid; i < Maxlocks; i += hostc) {
        freewtntspace(locks[i].wtntp);
    }
}

/**
 * @brief jia_lock -- get the lock
 *
 * @param lock
 */
void jia_lock(int lock) {
    int i;
#ifdef DOSTAT
    register unsigned int begin = get_usecs();
    if (statflag == 1) {
        jiastat.lockcnt++;
        jiastat.kernelflag = 1;
    }
#endif

    if (hostc == 1)
        return; // single host no need to use lock

    if (LOAD_BAL == ON) {
        endtime = jia_clock();
        caltime += (endtime - starttime);
    }

    sprintf(errstr, "lock %d should range from 0 to %d", lock, Maxlocks - 1);
    assert(((lock >= 0) && (lock < Maxlocks)), errstr);

    for (i = 0; i <= stackptr; i++)
        assert((lockstack[i].lockid != lock),
               "Embeding of the same lock is not allowed!");

    endinterval(ACQ);

    acqwait = 1;
    acquire(lock);

    startinterval(ACQ);

    pushstack(lock);

    if (LOAD_BAL == ON)
        starttime = jia_clock();

#ifdef DOSTAT
    if (statflag == 1) {
        jiastat.locktime += get_usecs() - begin;
        jiastat.kernelflag = 0;
    }
#endif
}

/**
 * @brief jia_unlock --
 *
 * @param lock
 */
void jia_unlock(int lock) {
#ifdef DOSTAT
    register unsigned int begin = get_usecs();
    if (statflag == 1) {
        jiastat.kernelflag = 1;
    }
#endif
    if (hostc == 1)
        return;

    if (LOAD_BAL == ON) {
        endtime = jia_clock();
        caltime += (endtime - starttime);
    }

    sprintf(errstr, "lock %d should range from 0 to %d", lock, Maxlocks - 1);
    assert(((lock >= 0) && (lock < Maxlocks)), errstr);

    assert((lock == top.lockid), "lock and unlock should be used in pair!");

    endinterval(REL);

    sendwtnts(REL);

    startinterval(REL);

    popstack();

    if (LOAD_BAL == ON)
        starttime = jia_clock();

#ifdef DOSTAT
    if (statflag == 1) {
        jiastat.unlocktime += get_usecs() - begin;
        jiastat.kernelflag = 0;
    }
#endif
}

/**
 * @brief jia_barrier -- end an interval and start next
 *
 */
void jia_barrier() {
#ifdef DOSTAT
    register unsigned int begin = get_usecs();
    if (statflag == 1) {
        jiastat.barrcnt++;
        jiastat.kernelflag = 1;
    }
#endif
    if (hostc == 1)
        return; // single machine

    if (LOAD_BAL == ON) {
        endtime = jia_clock();
        caltime += (endtime - starttime);
    }

    assert((stackptr == 0), "barrier can not be used in CS!");

    VERBOSE_LOG(3, "\nEnter jia barrier\n");
    endinterval(BARR);

    if (H_MIG == ON) {
        migcheckcache();
    }

    barrwait = 1;
    sendwtnts(BARR);
    freewtntspace(top.wtntp);
    while (barrwait)
        ;
    VERBOSE_LOG(3, "555555555555555\n");
    if ((H_MIG == ON) && (W_VEC == ON)) {
        jia_wait();
    }
    startinterval(BARR);
    VERBOSE_LOG(3, "66666666666666\n");
    if (LOAD_BAL == ON)
        starttime = jia_clock();

#ifdef DOSTAT
    if (statflag == 1) {
        jiastat.barrtime += get_usecs() - begin;
        jiastat.kernelflag = 0;
    }
#endif
    VERBOSE_LOG(3, "jia_barrier completed\n");
}

/**
 * @brief endinterval -- end an interval, save the changes and send them to
 * their home page
 *
 * @param synop syn operation
 */
void endinterval(int synop) {
    // register advise compiler store these variables into registers
    register int cachei;
    register int pagei;
    register int hpages;
    VERBOSE_LOG(3, "Enter endinterval!\n");
    for (cachei = 0; cachei < Cachepages; cachei++) {
        if (cache[cachei].wtnt == 1) { // when cached page wtnt == 1, save it
            savepage(cachei);
        }
    }

    senddiffs();

    hpages = hosts[jia_pid].homesize / Pagesize; // page number of jia_pid host
    for (pagei = 0; pagei < hpages; pagei++) {
        if ((home[pagei].wtnt & 1) != 0) { // bit1 == 1
            if (home[pagei].rdnt != 0) {
                savewtnt(top.wtntp, home[pagei].addr, Maxhosts);
                if (synop == BARR)
                    home[pagei].rdnt = 0;
            }

            if ((W_VEC == ON) &&
                (home[pagei].wvfull == 0)) { // write vector handle
                int i;
                wtvect_t wv = WVNULL;
                for (i = 0; i < Pagesize; i += Blocksize) {
                    if (memcmp(home[pagei].addr + i, home[pagei].twin + i,
                               Blocksize) != 0) {
                        wv |= ((wtvect_t)1) << (i / Blocksize);
                    }
                }
                addwtvect(pagei, wv, jia_pid);
            }

        } /*if*/
    }     /*for*/
    while (diffwait)
        ; // wait all diffs were handled
    VERBOSE_LOG(3, "Out of endinterval!\n");
}

/**
 * @brief startinterval -- start an interval
 *
 * @param synop
 */
void startinterval(int synop) {
    register int cachei;
    register int pagei;
    register int hpages;
    char swcnt;

    for (cachei = 0; cachei < Cachepages; cachei++) {
        if (cache[cachei].wtnt == 1) {
            cache[cachei].wtnt = 0;
            memprotect((caddr_t)cache[cachei].addr, Pagesize, PROT_READ);
            cache[cachei].state = RO;
            freetwin(&(cache[cachei].twin));
        }
    }

    hpages = hosts[jia_pid].homesize / Pagesize;
    if ((synop != BARR) || (AD_WD != ON)) {
        for (pagei = 0; pagei < hpages; pagei++)
            if ((home[pagei].wtnt & 1) != 0) {
                home[pagei].wtnt &= 0xfe;
                memprotect((caddr_t)home[pagei].addr, Pagesize, PROT_READ);
            }
    } else {
        for (pagei = 0; pagei < hpages; pagei++)
            switch (home[pagei].wtnt & 7) {
            case 0: /*000, written by no one in last barr itvl*/
                break;
            case 1: /*001, this is impossible*/
            case 5: /*101, this is impossible*/
                assert(0, "This should not have happened! WTDT");
                break;
            case 2: /*010, written by only home host in last barr itvl, the page
                       is RO*/
                swcnt = (home[pagei].wtnt >> 4) & 0xf;
                swcnt++;
                if (swcnt >= SWvalve) {
                    memprotect((caddr_t)home[pagei].addr, Pagesize,
                               PROT_READ | PROT_WRITE);
                    home[pagei].wtnt |= 3;
                    if ((W_VEC == ON) && (home[pagei].wvfull == 0)) {
                        newtwin(&(home[pagei].twin));
                        memcpy(home[pagei].twin, home[pagei].addr, Pagesize);
                    }
                } else {
                    home[pagei].wtnt = (swcnt << 4) & 0xf0;
                }
                break;
            case 3: /*011, written by only home host in last barr itvl, the page
                       is RW*/
                swcnt = (home[pagei].wtnt >> 4) & 0xf;
                swcnt++;
                if (swcnt >= SWvalve) {
                    if ((W_VEC == ON) && (home[pagei].wvfull == 0)) {
                        newtwin(&(home[pagei].twin));
                        memcpy(home[pagei].twin, home[pagei].addr, Pagesize);
                    }
                } else {
                    home[pagei].wtnt = (swcnt << 4) & 0xf0;
                    memprotect((caddr_t)home[pagei].addr, Pagesize, PROT_READ);
                }
                break;
            case 4: /*100, written by only remote host in last barr itvl*/
            case 6: /*110, written by home and remote host in last barr itvl,
                       the page is RO*/
                home[pagei].wtnt = 0;
                break;
            case 7: /*111, written by home and remote host in last barr itvl,
                       the page is RW*/
                home[pagei].wtnt = 0;
                memprotect((caddr_t)home[pagei].addr, Pagesize, PROT_READ);
                break;
            } /*switch*/
    }         /*else*/
}

/**
 * @brief pushstack -- push lock into lockstack
 *
 * @param lock lock id
 */
void pushstack(int lock) {
    stackptr++;
    assert((stackptr < Maxstacksize), "Too many continuous ACQUIRES!");

    top.lockid = lock;
}

/**
 * @brief popstack -- pop the top lock in lockstack
 *
 * (save the unlucky one's write notice pairs (wtnts[i], from[i]) to the new top
 * one)
 *
 */
void popstack() {
    int wtnti;
    wtnt_t *wnptr;

    stackptr--;
    assert((stackptr >= -1), "More unlocks than locks!");

    if (stackptr >= 0) {
        wnptr = lockstack[stackptr + 1].wtntp;
        while (wnptr != WNULL) {
            for (wtnti = 0; wtnti < wnptr->wtntc; wtnti++)
                savewtnt(top.wtntp, wnptr->wtnts[wtnti], wnptr->from[wtnti]);
            wnptr = wnptr->more;
        }
    }

    freewtntspace(lockstack[stackptr + 1].wtntp);
}

/**
 * @brief acquire() -- send ACQ msg to lock%hostc to acquire the lock
 *
 * @param lock
 *
 * acq msg data: | lock |
 */
void acquire(int lock) {
    jia_msg_t *req;

    req = newmsg();

    req->op = ACQ;
    req->frompid = jia_pid;
    req->topid = lock % hostc; // it seems that lock was divided circularly
    req->scope = locks[lock].myscope;
    req->size = 0;
    appendmsg(req, ltos(lock), Intbytes);

    asendmsg(req); // send the ACQ msg the lock owner

    freemsg(req);
    while (acqwait)
        ;
}

/**
 * @brief sendwtnts() -- send wtnts
 *
 * @param operation BARR or REL
 *
 * BARR or REL message's data : | top.lockid(4bytes) | addr1 | ... | addrn |
 */
void sendwtnts(int operation) {
    int wtnti;
    jia_msg_t *req;
    wtnt_t *wnptr; // write notice pointer
    VERBOSE_LOG(3, "Enter sendwtnts!\n");
    req = newmsg();

    req->frompid = jia_pid;
    req->topid = top.lockid % hostc;
    req->size = 0;
    req->scope = (operation == REL) ? locks[hidelock].myscope
                                    : locks[top.lockid].myscope;
    appendmsg(req, ltos(top.lockid),
              Intbytes); // note here, after ltos transformation(8bytes),
                         // truncation here

    wnptr = top.wtntp;
    wnptr = appendstackwtnts(req, wnptr);

    VERBOSE_LOG(3, "req message frompid = %d, topid = %d\n", jia_pid, req->topid);
    VERBOSE_LOG(3, "req size is %d, req data is %s\n", req->size, req->data);
    VERBOSE_LOG(3, "wnptr == WNULL is %d\n", wnptr == WNULL ? 1 : 0);
    while (wnptr != WNULL) {
        req->op = WTNT;
        asendmsg(req);
        req->size = Intbytes; // TODO: Need to check
        wnptr = appendstackwtnts(req, wnptr);
    }
    req->op = operation;
    VERBOSE_LOG(3, "my pid is %d\n", jiapid);
    asendmsg(req);
    freemsg(req);
    VERBOSE_LOG(3, "\nOut of sendwtnts!\n\n");
}

/**
 * @brief savepage() -- save diff and wtnt
 *
 * @param cachei cached page index
 */
void savepage(int cachei) {
    savediff(cachei);
    savewtnt(top.wtntp, cache[cachei].addr, Maxhosts);
}

/**
 * @brief savewtnt -- save write notice about the page of the addr to ptr
 *
 * @param ptr lockstack[stackptr]'s wtnt_t pointer
 * @param addr the original address of cached page
 * @param frompid
 */
void savewtnt(wtnt_t *ptr, address_t addr, int frompid) {
    int wtnti;
    int exist;
    wtnt_t *wnptr;

    exist = 0;
    wnptr = ptr;

    while ((exist == 0) && (wnptr != WNULL)) {
        wtnti = 0;
        while ((wtnti < wnptr->wtntc) &&
               (((unsigned long)addr / Pagesize) !=
                ((unsigned long)wnptr->wtnts[wtnti] / Pagesize))) {
            wtnti++;
        }
        if (wtnti >= wnptr->wtntc) // if addr wan't found in current wtnt_t
                                   // object, go to next
            wnptr = wnptr->more;
        else
            exist = 1; // wtnt about the address has been found
    }

    if (exist == 0) { // addr's wtnt don't exist
        wnptr = ptr;
        while (wnptr->wtntc >= Maxwtnts) { //
            if (wnptr->more == WNULL)
                wnptr->more = newwtnt();
            wnptr = wnptr->more;
        }
        wnptr->wtnts[wnptr->wtntc] = addr;
        wnptr->from[wnptr->wtntc] = frompid;
        wnptr->wtntc++;
    } else {                                // wtnt found
        if (ptr == locks[hidelock].wtntp) { /*barrier*/
            wnptr->from[wtnti] = Maxhosts;
        } else {
            wnptr->from[wtnti] = frompid; /*lock or stack*/
        }
    }
}

/**
 * @brief appendstackwtnts -- append wtnt_t wtnts(i.e addr) to msg as many as
 * possible
 *
 * @param msg msg that include ptr->wtnts
 * @param ptr write notice pointer
 * @return wtnt_t*
 */
wtnt_t *appendstackwtnts(jia_msg_t *msg, wtnt_t *ptr) {
    int full; // flag indicate that msg full or not
    wtnt_t *wnptr;

    full = 0;
    wnptr = ptr;
    while ((wnptr != WNULL) && (full == 0)) {
        // if ((msg->size+(wnptr->wtntc*Intbytes)) < Maxmsgsize) {
        if ((msg->size + (wnptr->wtntc * (sizeof(unsigned char *)))) <
            Maxmsgsize) {
            // appendmsg(msg, wnptr->wtnts, (wnptr->wtntc*Intbytes));
            appendmsg(msg, wnptr->wtnts,
                      (wnptr->wtntc) * (sizeof(unsigned char *)));
            wnptr = wnptr->more;
        } else {
            full = 1;
        }
    }
    return (wnptr);
}

/**
 * @brief appendlockwtnts -- append lock's wtnts to msg
 *
 * @param msg
 * @param ptr
 * @param acqscope
 * @return wtnt_t*
 */
wtnt_t *appendlockwtnts(jia_msg_t *msg, wtnt_t *ptr, int acqscope) {
    int wtnti;
    int full;
    wtnt_t *wnptr;

    full = 0;
    wnptr = ptr;
    while ((wnptr != WNULL) && (full == 0)) {
        if ((msg->size + (wnptr->wtntc * (sizeof(unsigned char *)))) <
            Maxmsgsize) {
            for (wtnti = 0; wtnti < wnptr->wtntc; wtnti++)
                if ((wnptr->from[wtnti] > acqscope) &&
                    (homehost(wnptr->wtnts[wtnti]) != msg->topid))
                    // appendmsg(msg,ltos(wnptr->wtnts[wtnti]),Intbytes);
                    appendmsg(msg, ltos(wnptr->wtnts[wtnti]),
                              sizeof(unsigned char *));
            wnptr = wnptr->more;
        } else {
            full = 1;
        }
    }
    return (wnptr);
}

/**
 * @brief grantlock -- grant the acquire lock msg request
 *
 * @param lock
 * @param toproc
 * @param acqscope
 *
 * ACQGRANT msg data : | lock | addr1 | ... | addrn |
 */
void grantlock(int lock, int toproc, int acqscope) {
    jia_msg_t *grant;
    wtnt_t *wnptr;

    grant = newmsg();

    grant->frompid = jia_pid;
    grant->topid = toproc;
    grant->scope = locks[lock].scope;
    grant->size = 0;

    appendmsg(grant, ltos(lock), Intbytes);

    wnptr = locks[lock].wtntp;
    wnptr = appendlockwtnts(grant, wnptr, acqscope);
    while (wnptr != WNULL) { // current msg is full, but still wtnts  left
        grant->op = INVLD;
        asendmsg(grant);
        grant->size = Intbytes;
        wnptr = appendlockwtnts(grant, wnptr, acqscope);
    }

    grant->op = ACQGRANT;
    asendmsg(grant);

    freemsg(grant);
}

/**
 * @brief acqserver -- ACQ msg server
 *
 * @param req ACQ msg
 *
 * ACQ msg data : | lock |
 */
void acqserver(jia_msg_t *req) {
    int lock;
    int wtnti;

    assert((req->op == ACQ) && (req->topid == jia_pid),
           "Incorrect ACQ message!");

    lock = (int)stol(req->data); // get the lock
    assert((lock % hostc == jia_pid), "Incorrect home of lock!");

    locks[lock].acqs[locks[lock].acqc] = req->frompid;
    locks[lock].acqscope[locks[lock].acqc] = req->scope;
    locks[lock].acqc++;

    if (locks[lock].acqc == 1)
        grantlock(lock, locks[lock].acqs[0], locks[lock].acqscope[0]);
}

/**
 * @brief relserver -- REL msg server
 *
 * @param req request msg REL
 *
 * REL msg data : | lock | addr | ... | addr
 */
void relserver(jia_msg_t *req) {
    int lock;
    int acqi;

    assert((req->op == REL) && (req->topid == jia_pid),
           "Incorrect REL Message!");

    lock = (int)stol(req->data); // get the lock
    assert((lock % hostc == jia_pid), "Incorrect home of lock!");
    assert((req->frompid == locks[lock].acqs[0]),
           "This should not have happened! 6");

    if (req->scope > locks[hidelock].myscope)
        noclearlocks = 1;

    recordwtnts(req);
    locks[lock].scope++;

    for (acqi = 0; acqi < (locks[lock].acqc - 1); acqi++) {
        locks[lock].acqs[acqi] = locks[lock].acqs[acqi + 1];
        locks[lock].acqscope[acqi] = locks[lock].acqscope[acqi + 1];
    }
    locks[lock].acqc--;

    if (locks[lock].acqc > 0)
        grantlock(lock, locks[lock].acqs[0], locks[lock].acqscope[0]);
}

/**
 * @brief appendbarrwtnts -- append wtnt_t ptr's pair (wtnts, from) to msg
 *
 * @param msg grantbarr msg
 * @param ptr lock's write notice's pointer
 * @return wtnt_t*
 */
wtnt_t *appendbarrwtnts(jia_msg_t *msg, wtnt_t *ptr) {
    int wtnti;
    int full;
    wtnt_t *wnptr;

    full = 0;
    wnptr = ptr;
    while ((wnptr != WNULL) && (full == 0)) {
        // if ((msg->size+(wnptr->wtntc*Intbytes*2))<Maxmsgsize){
        if ((msg->size + (wnptr->wtntc *
                          (Intbytes + sizeof(unsigned char *)))) < Maxmsgsize) {
            for (wtnti = 0; wtnti < wnptr->wtntc; wtnti++) {
                // appendmsg(msg,ltos(wnptr->wtnts[wtnti]),Intbytes);
                appendmsg(msg, ltos(wnptr->wtnts[wtnti]),
                          sizeof(unsigned char *));
                appendmsg(msg, ltos(wnptr->from[wtnti]), Intbytes);
            }
            wnptr = wnptr->more;
        } else {
            full = 1;
        }
    }
    return (wnptr);
}

/**
 * @brief broadcast -- broadcast msg to all hosts
 *
 * @param msg msg that will be broadcast
 */
void broadcast(jia_msg_t *msg) {
    int hosti;

    if (B_CAST == OFF) {
        for (hosti = 0; hosti < hostc; hosti++) {
            msg->topid = hosti;
            asendmsg(msg);
        }
    } else {
        bsendmsg(msg);
    }
}

/**
 * @brief grantbarr -- grant barr request msg
 *
 * @param lock
 *
 * grantbarr msg data: | lock(4bytes)  | maybe (addr (8bytes)) , from(4bytes)
 */
void grantbarr(int lock) {
    jia_msg_t *grant;
    wtnt_t *wnptr;
    int hosti;

    grant = newmsg();

    grant->frompid = jia_pid;
    grant->topid = jia_pid;
    grant->scope = locks[lock].scope;
    grant->size = 0;

    appendmsg(grant, ltos(lock), Intbytes); // encapsulate the lock

    wnptr = locks[lock].wtntp; // get lock's write notice list's head
    wnptr = appendbarrwtnts(grant,
                            wnptr); // append lock's write notice contents(addr,
                                    // from) to msg if possible
    while (wnptr != WNULL) {
        grant->op = INVLD;
        broadcast(grant);
        grant->size = Intbytes; // lock
        wnptr = appendbarrwtnts(grant, wnptr);
    }
    grant->op = BARRGRANT;
    broadcast(grant);
    freemsg(grant);
}

/**
 * @brief barrserver -- barr msg server
 *
 * @param req barr msg
 * barr msg data: | lock (4bytes) |
 */
void barrserver(jia_msg_t *req) {
    VERBOSE_LOG(3, "host %d is running in barrserver\n", jiapid);
    int lock;

    assert((req->op == BARR) && (req->topid == jia_pid),
           "Incorrect BARR Message!");

    lock = (int)stol(req->data);

    assert((lock % hostc == jia_pid), "Incorrect home of lock!");
    assert((lock == hidelock), "This should not have happened! 8");

    recordwtnts(req); // record write notices in msg barr's data

    locks[lock].acqc++;

#ifdef DEBUG
    VERBOSE_LOG(3, "barrier count=%d, from host %d\n", locks[lock].acqc, req->frompid);
#endif
    VERBOSE_LOG(3, "locks[%d].acqc = %d\n", lock, locks[lock].acqc);
    if (locks[lock].acqc == hostc) {
        locks[lock].scope++;
        grantbarr(lock);
        locks[lock].acqc = 0;
        freewtntspace(locks[lock].wtntp);
    }
    VERBOSE_LOG(3, "host %d is out of barrserver\n", jiapid);
}

/**
 * @brief recordwtnts -- record write notices in msg req's data
 *
 * @param req request msg
 */
void recordwtnts(jia_msg_t *req) {
    int lock;
    int datai;

    lock = (int)stol(req->data); // get the lock
    if (lock != hidelock) {      /*lock*/
        for (datai = Intbytes; datai < req->size;
             datai += sizeof(unsigned char *))
            savewtnt(locks[lock].wtntp, (address_t)stol(req->data + datai),
                     locks[lock].scope);
    } else { /*barrier*/
        for (datai = Intbytes; datai < req->size;
             datai += sizeof(unsigned char *))
            savewtnt(locks[lock].wtntp, (address_t)stol(req->data + datai),
                     req->frompid);
    }
}

/**
 * @brief wtntserver -- msg wtnt server
 *
 * @param req
 *
 * wtnt msg: | lock | addr | addr |...|
 */
void wtntserver(jia_msg_t *req) {
    int lock;

    assert((req->op == WTNT) && (req->topid == jia_pid),
           "Incorrect WTNT Message!");

    lock = (int)stol(req->data);
    assert((lock % hostc == jia_pid), "Incorrect home of lock!");

    recordwtnts(req);
}

/**
 * @brief migcheckcache --
 *
 */
void migcheckcache() {
    wtnt_t *wnptr;
    int wtnti, cachei;
    unsigned long addr;

    wnptr = top.wtntp;
    while (wnptr != WNULL) {
        for (wtnti = 0; (wtnti < wnptr->wtntc); wtnti++) {
            addr = (unsigned long)wnptr->wtnts[wtnti];
            cachei = page[(addr - Startaddr) / Pagesize].cachei;
            if ((cache[cachei].state == RO) || (cache[cachei].state == RW)) {
                addr++;
                wnptr->wtnts[wtnti] = (address_t)addr;
            }
        }
        wnptr = wnptr->more;
    }
}

void migarrangehome() {
    int end, i, homei;

    end = 0;
    for (i = 0; ((i < Homepages) && (end == 0)); i++) {
        if (home[i].addr == (address_t)0) {
            for (homei = i + 1;
                 (homei < Homepages) && (home[homei].addr == (address_t)0);
                 homei++)
                ;
            if (homei < Homepages) {
                page[((unsigned int)home[homei].addr - Startaddr) / Pagesize]
                    .homei = i;
                home[i].addr = home[homei].addr;
                home[i].wtnt = home[homei].wtnt;
                home[i].rdnt = home[homei].rdnt;
                home[homei].addr = (address_t)0;
                if (W_VEC == ON) {
                    wtvect_t *temp;
                    temp = home[i].wtvect;
                    home[i].wtvect = home[homei].wtvect;
                    home[homei].wtvect = temp;
                    home[i].wvfull = home[homei].wvfull;
                }
            } else {
                end = 1;
            }
        }
    }
    hosts[jia_pid].homesize = (i - 1) * Pagesize;
    // VERBOSE_LOG(3, "New homepages=%d\n",hosts[jia_pid].homesize/Pagesize);
}

/**
 * @brief migpage --
 *
 * @param addr
 * @param frompid
 * @param topid
 */
void migpage(unsigned long addr, int frompid, int topid) {
    int pagei, homei, cachei;

    pagei = (addr - Startaddr) / Pagesize;
    /*
     VERBOSE_LOG(3, "Mig page 0x%x from host %d to %d\n",pagei,frompid,topid);
    */

    if (topid == jia_pid) { /*New Home*/
        cachei = page[pagei].cachei;
        for (homei = 0;
             (homei < Homepages) && (home[homei].addr != (address_t)0); homei++)
            ;

        if (homei < Homepages) {
            home[homei].addr = (address_t)addr;
            home[homei].wtnt = (cache[cachei].wtnt == 0) ? 2 : 3;
            home[homei].rdnt = 1;
            if (W_VEC == ON)
                setwtvect(homei, WVFULL);
            page[pagei].homei = homei;
        } else {
            assert(0, "Home exceed in home migration");
        }

        if (cachei < Cachepages) { /*Old Cache*/
            /*memprotect((caddr_t)addr,Pagesize,PROT_READ); */
            page[pagei].cachei = Cachepages;
            if (cache[cachei].state == RW)
                freetwin(&(cache[cachei].twin));
            cache[cachei].state = UNMAP;
            cache[cachei].wtnt = 0;
            cache[cachei].addr = 0;
        } else {
            assert(0, "This should not have happened---MIG");
        }
#ifdef DOSTAT
        if (statflag == 1) {
            jiastat.migincnt++;
        }
#endif
    } else if (frompid == jia_pid) { /*Old Home*/
        homei = homepage(addr);
        assert((unsigned long)home[homei].addr == addr, "MIG ERROR");

        for (cachei = 0;
             ((cachei < Cachepages) && (cache[cachei].state != UNMAP) &&
              (cache[cachei].state != INV));
             cachei++)
            ;

        if (cachei < Cachepages) { /*New Cache*/
            if (cache[cachei].state == INV)
                flushpage(cachei);
            cache[cachei].state = RO;
            cache[cachei].wtnt = home[homei].wtnt & 1;
            cache[cachei].addr = (address_t)addr;
            page[pagei].cachei = cachei;
        } else {
            memunmap((address_t)addr, Pagesize);
        }

        home[homei].wtnt = 0;
        home[homei].rdnt = 0;
        home[homei].addr = (address_t)0;
        if (W_VEC == ON)
            setwtvect(homei, WVFULL);
        page[pagei].homei = Homepages;

#ifdef DOSTAT
        if (statflag == 1) {
            jiastat.migoutcnt++;
        }
#endif
    }
    page[pagei].homepid = topid;
}

/**
 * @brief invalidate --
 *
 * @param req
 */
void invalidate(jia_msg_t *req) {
    int cachei, seti;
    int lock;
    int datai;
    address_t addr;
    int migtag;
    int from;
    int homei, pagei;

    lock = (int)stol(req->data); // get the lock
    datai = Intbytes;

    while (datai < req->size) {
        addr = (address_t)stol(req->data + datai); // get the addr
        if (H_MIG == ON) {
            migtag = ((unsigned long)addr) % Pagesize;
            addr = (address_t)(((unsigned long)addr / Pagesize) * Pagesize);
        }
        // datai+=Intbytes;
        datai += sizeof(unsigned char *);

        if (lock == hidelock) { /*Barrier*/
            from = (int)stol(req->data + datai);
            datai += Intbytes;
        } else { /*Lock*/
            from = Maxhosts;
        }

        if ((from != jia_pid) && (homehost(addr) != jia_pid)) {
            cachei =
                (int)page[((unsigned long)addr - Startaddr) / Pagesize].cachei;
            if (cachei < Cachepages) {
                if (cache[cachei].state != INV) {
                    if (cache[cachei].state == RW)
                        freetwin(&(cache[cachei].twin));
                    cache[cachei].wtnt = 0;
                    cache[cachei].state = INV;
                    memprotect((caddr_t)cache[cachei].addr, Pagesize,
                               PROT_NONE);
#ifdef DOSTAT
                    if (statflag == 1) {
                        jiastat.invcnt++;
                    }
#endif
                }
            }
        }

        if ((H_MIG == ON) && (lock == hidelock) && (from != Maxhosts) &&
            (migtag != 0)) {
            migpage((unsigned long)addr, homehost(addr), from);
        }

        if ((AD_WD == ON) && (lock == hidelock) &&
            (homehost(addr) == jia_pid) && (from != jia_pid)) {
            homei = homepage(addr);
            home[homei].wtnt |= 4;
        }

    } /*while*/
}

/**
 * @brief invserver -- INVLD msg server
 *
 * @param req INVID msg
 */
void invserver(jia_msg_t *req) {
    assert((req->op == INVLD) && (req->topid == jia_pid),
           "Incorrect INVLD Message!");

    invalidate(req);
}

/**
 * @brief barrgrantserver -- barrgrant msg server
 *
 * @param req BARRGRANT msg
 */
void barrgrantserver(jia_msg_t *req) {
    int lock;

    assert((req->op == BARRGRANT) && (req->topid == jia_pid),
           "Incorrect BARRGRANT Message!");

    if (noclearlocks == 0)
        clearlocks();
    invalidate(req);

    if (H_MIG == ON) {
        migarrangehome();
    }

    lock = (int)stol(req->data);
    locks[lock].myscope = req->scope;
    barrwait = 0;
    noclearlocks = 0;
}

/**
 * @brief acqgrantserver -- ACQGRANT msg server
 *
 * @param req ACQGRANT msg
 */
void acqgrantserver(jia_msg_t *req) {
    int lock;

    assert((req->op == ACQGRANT) && (req->topid == jia_pid),
           "Incorrect ACQGRANT Message!");
    invalidate(req);

    lock = (int)stol(req->data);
    locks[lock].myscope = req->scope;
    acqwait = 0;
}

/************Conditional Variable Part****************/

void jia_setcv(int cvnum) {
    jia_msg_t *req;

    if (hostc == 1)
        return;

    sprintf(errstr, "condv %d should range from 0 to %d", cvnum, Maxcvs - 1);
    assert(((cvnum >= 0) && (cvnum < Maxcvs)), errstr);

    req = newmsg();
    req->op = SETCV;
    req->frompid = jia_pid;
    req->topid = cvnum % hostc;
    req->size = 0;
    appendmsg(req, ltos(cvnum), Intbytes);

    asendmsg(req);

    freemsg(req);
}

void jia_resetcv(int cvnum) {
    jia_msg_t *req;

    if (hostc == 1)
        return;

    sprintf(errstr, "condv %d should range from 0 to %d", cvnum, Maxcvs - 1);
    assert(((cvnum >= 0) && (cvnum < Maxcvs)), errstr);

    req = newmsg();
    req->op = RESETCV;
    req->frompid = jia_pid;
    req->topid = cvnum % hostc;
    req->size = 0;
    appendmsg(req, ltos(cvnum), Intbytes);

    asendmsg(req);

    freemsg(req);
}

void jia_waitcv(int cvnum) {
    jia_msg_t *req;
    int lockid;

    if (hostc == 1)
        return;

    sprintf(errstr, "condv %d should range from 0 to %d", cvnum, Maxcvs - 1);
    assert(((cvnum >= 0) && (cvnum < Maxcvs)), errstr);

    req = newmsg();
    req->op = WAITCV;
    req->frompid = jia_pid;
    req->topid = cvnum % hostc;
    req->size = 0;
    appendmsg(req, ltos(cvnum), Intbytes);

    cvwait = 1;

    asendmsg(req);

    freemsg(req);
    while (cvwait)
        ;
}

/**
 * @brief jia_wait -- send WAIT msg to master and wait for WAITGRANT reply to be
 * served
 *
 */
void jia_wait() {
    jia_msg_t *req;

    if (hostc == 1)
        return;

    if (LOAD_BAL == ON) {
        endtime = jia_clock();
        caltime += (endtime - starttime);
    }

    req = newmsg();
    req->frompid = jia_pid;
    req->topid = 0;
    req->op = WAIT;
    req->size = 0;

    waitwait = 1;
    asendmsg(req);
    freemsg(req);
    while (waitwait)
        ;

    if (LOAD_BAL == ON)
        starttime = jia_clock();
}

/**
 * @brief waitserver -- WAIT msg request server
 *
 * @param req WAIT msg request
 */
void waitserver(jia_msg_t *req) {
    jia_msg_t *grant;
    int i;

    assert((req->op == WAIT) && (req->topid == jia_pid),
           "Incorrect WAIT Message!");

    waitcounter++;

    if (waitcounter == hostc) {
        grant = newmsg();
        waitcounter = 0;
        grant->frompid = jia_pid;
        grant->size = 0;
        grant->op = WAITGRANT;
        broadcast(grant);
        freemsg(grant);
    }
}

/**
 * @brief waitgrantserver -- waitgrant msg server
 *
 * @param req WAITGRANT msg request
 */
void waitgrantserver(jia_msg_t *req) {
    assert((req->op == WAITGRANT) && (req->topid == jia_pid),
           "Incorrect WAITGRANT Message!");

    waitwait = 0;
}

/**
 * @brief grantcondv -- append condv to CVGRANT msg and send it to toproc
 *
 * @param condv condition variable
 * @param toproc destination host
 */
void grantcondv(int condv, int toproc) {
    jia_msg_t *grant;

    grant = newmsg();
    grant->op = CVGRANT;
    grant->frompid = jia_pid;
    grant->topid = toproc;
    grant->size = 0;
    appendmsg(grant, ltos(condv), Intbytes);

    asendmsg(grant);

    freemsg(grant);
}

/**
 * @brief setcvserver -- setcv msg server
 *
 * @param req SETCV msg request
 */
void setcvserver(jia_msg_t *req) {
    int condv;
    int i;

    assert((req->op == SETCV) && (req->topid == jia_pid),
           "Incorrect SETCV Message!");

    condv = (int)stol(req->data);
    assert((condv % hostc == jia_pid), "Incorrect home of condv!");

    condvars[condv].value = 1;

    if (condvars[condv].waitc > 0) {
        for (i = 0; i < condvars[condv].waitc; i++)
            grantcondv(condv, condvars[condv].waits[i]);
        condvars[condv].waitc = 0;
    }
}

/**
 * @brief resetcvserver -- resetcv msg server
 *
 * @param req resetcv msg
 */
void resetcvserver(jia_msg_t *req) {
    int condv;

    assert((req->op == RESETCV) && (req->topid == jia_pid),
           "Incorrect RESETCV Message!");
    condv = (int)stol(req->data); // get the condition variable
    condvars[condv].value = 0;
}

/**
 * @brief waitcvserver -- if req->op
 *
 * @param req
 */
void waitcvserver(jia_msg_t *req) {
    int condv;
    int i;

    assert((req->op == WAITCV) && (req->topid == jia_pid),
           "Incorrect WAITCV Message!");

    condv = (int)stol(req->data);

    condvars[condv].waits[condvars[condv].waitc] = req->frompid;
    condvars[condv].waitc++;

    if (condvars[condv].value == 1) {
        for (i = 0; i < condvars[condv].waitc; i++)
            grantcondv(condv, condvars[condv].waits[i]);
        condvars[condv].waitc = 0;
    }
}

/**
 * @brief cvgrantserver -- if msg req's op == CVGRANT and req->topid == jia_pid,
 * set cvwait = 0
 *
 * @param req jia msg
 */
void cvgrantserver(jia_msg_t *req) {
    int condv;

    assert((req->op == CVGRANT) && (req->topid == jia_pid),
           "Incorrect CVGRANT Message!");
    condv = (int)stol(req->data);

    // ...
    cvwait = 0;
}

#else /* NULL_LIB */
void jia_lock(int lock) {}

void jia_unlock(int lock) {}

void jia_barrier() {}

void jia_wait() {}

void jia_setcv(int lock) {}

void jia_resetcv(int lock) {}

void jia_waitcv() {}

#endif /* NULL_LIB */
