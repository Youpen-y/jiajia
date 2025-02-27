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
 **********************************************************************/

#include "syn.h"
#include "comm.h"
#include "global.h"
#include "init.h"
#include "jia.h"
#include "mem.h"
#include "msg.h"
#include "setting.h"
#include "stat.h"
#include "tools.h"
#include <stdatomic.h>

/* mem */
extern jiapage_t page[Maxmempages];
extern jiacache_t cache[Cachepages];
extern jiahome_t home[Homepages];
extern _Atomic volatile int diffwait;

/* tools */
extern int H_MIG, AD_WD, W_VEC, B_CAST;

#ifdef DOSTAT
extern int statflag;
extern jiastat_t jiastat;
#endif

/* syn */
// lock array, according to the hosts allocate lock(eg.
// host0: 0, 2,... 62; host1 = 1, 3, ..., 63)
jialock_t locks[Maxlocks + 1];      // the last one is a barrier(hidelock)
jiastack_t lockstack[Maxstacksize]; // lock stack
jiacv_t condvars[Maxcvs];
int stackptr;

_Atomic volatile int waitwait, acqwait, barrwait, cvwait;
volatile int noclearlocks;
volatile int waitcounter;

static wtnt_t *appendbarrwtnts(jia_msg_t *msg, wtnt_t *ptr);
static wtnt_t *appendlockwtnts(jia_msg_t *msg, wtnt_t *ptr, int acqscope);
static wtnt_t *appendstackwtnts(jia_msg_t *msg, wtnt_t *ptr);

/**
 * @brief initsyn -- initialize the sync setting
 *
 */
void initsyn() {
#ifdef DOSTAT
    register unsigned int begin = get_usecs();
#endif

    int i, j, k;

    /** step 1: init locks */
    for (i = 0; i <= Maxlocks; i++) {
        atomic_init(&locks[i].acqc, 0);
        locks[i].scope = 0;
        locks[i].myscope = -1;
        for (j = 0; j < Maxhosts; j++) {
            locks[i].acqs[j] = -1;
            locks[i].acqscope[j] = -1;
        }
        if ((i % system_setting.hostc) == system_setting.jia_pid)
            locks[i].wtntp = newwtnt();
        else
            locks[i].wtntp = WNULL;
    }

    /** step 2: init lockstack */
    for (i = 0; i < Maxstacksize; i++) {
        lockstack[i].lockid = 0;
        lockstack[i].wtntp = newwtnt();
    }

    stackptr = 0;
    top.lockid = hidelock; // lockstack[0].lockid = 64
    noclearlocks = 0;

    /** step 3: init condition variables */
    for (i = 0; i < Maxcvs; i++) {
        condvars[i].waitc = 0;
        for (j = 0; j < Maxhosts; j++) {
            condvars[i].waits[j] = -1;
        }
    }
    cvwait = 0;
    waitcounter = 0;

#ifdef DOSTAT
    jiastat.initsyn += get_usecs() - begin;
#endif
}

/**
 * @brief endinterval -- end an interval, save the diffs and wtnts of lock and send them to
 * their home page
 *
 * @param synop syn operation ACQ/BARR
 */
void endinterval(int synop) {
    register int cachei;
    register int pagei;
    register int hpages;
    log_info(3, "Enter endinterval!");

    /** step 1: save the diffs between cached page's and their twin */
    for (cachei = 0; cachei < Cachepages; cachei++) {
        if (cache[cachei].wtnt == 1) { //
            savepage(cachei);
        }
    }

    /** step 2: send diffs to their host */
    senddiffs();

    /** step 3: save all home page wtnts */
    hpages = system_setting.hosts[system_setting.jia_pid].homesize / Pagesize; // page number of jia_pid host
    for (pagei = 0; pagei < hpages; pagei++) {
        // home[pagei].wtnt & 1: home host has written this homepage
        if ((home[pagei].wtnt & 1) != 0) {
            // home[pagei].rdnt != 0: remote host has valid copy of this homepage(cachepage)
            if (home[pagei].rdnt != 0) {
                // remote host && home host has different copy, so we will savewtnts to record it
                // local host's wtnt.from and wtnt.scope is meaningless, mig must be zero as init
                savewtnt(top.wtntp, home[pagei].addr, 0, system_setting.jia_pid, Maxhosts);
                if (synop == BARR)
                    home[pagei].rdnt = 0;
            }

            if ((W_VEC == ON) && (home[pagei].wvfull == 0)) { // write vector handle
                int i;
                wtvect_t wv = WVNULL;
                for (i = 0; i < Pagesize; i += Blocksize) {
                    if (memcmp(home[pagei].addr + i, home[pagei].twin + i, Blocksize) != 0) {
                        wv |= ((wtvect_t)1) << (i / Blocksize);
                    }
                }
                addwtvect(pagei, wv, system_setting.jia_pid);
            }
        }
    }
    while (atomic_load(&diffwait))
        ; // wait all diffs were handled
    log_info(3, "Out of endinterval!");
}

/**
 * @brief startinterval -- start an interval
 *
 * @param synop: ACQ/REL/BARR
 */
void startinterval(int synop) {
    register int cachei;
    register int pagei;
    register int hpages;
    char swcnt;

    /** step 1: */
    for (cachei = 0; cachei < Cachepages; cachei++) {
        if (cache[cachei].wtnt == 1) {
            cache[cachei].wtnt = 0;
            memprotect((caddr_t)cache[cachei].addr, Pagesize, PROT_READ);
            cache[cachei].state = RO;
            freetwin(&(cache[cachei].twin));
        }
    }

    /** step 2: */
    hpages = system_setting.hosts[system_setting.jia_pid].homesize / Pagesize;
    if ((synop != BARR) || (AD_WD != ON)) {
        for (pagei = 0; pagei < hpages; pagei++)
            if (home[pagei].wtnt & 1) {
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
                jia_assert(0, "This should not have happened! WTDT");
                break;
            case 2: /*010, written by only home host in last barr itvl, the page
                       is RO*/
                swcnt = (home[pagei].wtnt >> 4) & 0xf;
                swcnt++;
                if (swcnt >= SWvalve) {
                    memprotect((caddr_t)home[pagei].addr, Pagesize, PROT_READ | PROT_WRITE);
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

/************lock Part****************/

/**
 * @brief clearlocks() -- free the write notice space that used by lock
 *
 */
void clearlocks() {
    int i;

    for (i = system_setting.jia_pid; i < Maxlocks; i += system_setting.hostc) {
        freewtntspace(locks[i].wtntp);
    }
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
    slot_t *slot;

    slot = freemsg_lock(&msg_buffer);
    req = &slot->msg;
    req->op = ACQ;
    req->frompid = system_setting.jia_pid;
    req->topid = lock % system_setting.hostc; // it seems that lock was divided circularly
    req->scope = locks[lock].myscope;
    req->size = 0;
    appendmsg(req, ltos(lock), Intbytes);

    move_msg_to_outqueue(slot, &outqueue);
    freemsg_unlock(slot);
    while (atomic_load(&acqwait))
        ;
    log_info(3, "acquire lock!!!");
}

/**
 * @brief pushstack -- push lock into lockstack
 *
 * @param lock lock id
 */
void pushstack(int lock) {
    stackptr++;
    jia_assert((stackptr < Maxstacksize), "Too many continuous ACQUIRES!");

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
    jia_assert((stackptr >= -1), "More unlocks than locks!");

    if (stackptr >= 0) {
        wnptr = lockstack[stackptr + 1].wtntp;
        while (wnptr != WNULL) {
            for (wtnti = 0; wtnti < wnptr->wtntc; wtnti++)
                // local host's wtnt.from and wtnt.scope is meaningless, mig is inherited
                savewtnt(top.wtntp, wnptr->wtnts[wtnti], wnptr->mig[wtnti], wnptr->from[wtnti], wnptr->scope[wtnti]);
            wnptr = wnptr->more;
        }
    }

    freewtntspace(lockstack[stackptr + 1].wtntp);
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
    slot_t *slot;

    slot = freemsg_lock(&msg_buffer);
    grant = &slot->msg;
    grant->frompid = system_setting.jia_pid;
    grant->topid = toproc;
    grant->scope = locks[lock].scope;
    grant->size = 0;

    appendmsg(grant, ltos(lock), Intbytes);

    wnptr = locks[lock].wtntp;
    wnptr = appendlockwtnts(grant, wnptr, acqscope);
    while (wnptr != WNULL) { // current msg is full, but still wtnts  left
        grant->op = INVLD;
        move_msg_to_outqueue(slot, &outqueue);
        grant->size = Intbytes;
        wnptr = appendlockwtnts(grant, wnptr, acqscope);
    }

    grant->op = ACQGRANT;

    move_msg_to_outqueue(slot, &outqueue);
    freemsg_unlock(slot);
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

    /* wnptr is not NULL and a msg is not full */
    while ((wnptr != WNULL) && (full == 0)) {
        if ((msg->size + (2*Intbytes + wnptr->wtntc * (sizeof(unsigned char *)))) < Maxmsgsize) {
            for (wtnti = 0; wtnti < wnptr->wtntc; wtnti++)
                if ((wnptr->scope[wtnti] > acqscope) &&
                    // if homehost(wnptr->wtnts[wtnti]) == msg->topid), not
                    // send(remote host is the owner of this copy)
                    (homehost(wnptr->wtnts[wtnti]) != msg->topid)) {
                    appendmsg(msg, ltos(wnptr->wtnts[wtnti]), sizeof(unsigned char *));
                    appendmsg(msg, ltos(wnptr->from[wtnti]), Intbytes);
                    appendmsg(msg, ltos(wnptr->mig[wtnti]), Intbytes);
                }
            wnptr = wnptr->more;
        } else {
            full = 1;
        }
    }
    return (wnptr);
}

/************Barrier Part****************/

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
    slot_t *slot;

    slot = freemsg_lock(&msg_buffer);
    grant = &slot->msg;

    grant->frompid = system_setting.jia_pid;
    grant->topid = system_setting.jia_pid;
    grant->scope = locks[lock].scope;
    grant->size = 0;

    appendmsg(grant, ltos(lock), Intbytes); // encapsulate the lock

    wnptr = locks[lock].wtntp; // get lock's write notice list's head
    wnptr = appendbarrwtnts(grant,
                            wnptr); // append lock's write notice contents(addr,
                                    // from) to msg if possible
    while (wnptr != WNULL) {
        grant->op = INVLD;
        broadcast(slot);
        grant->size = Intbytes; // lock
        wnptr = appendbarrwtnts(grant, wnptr);
    }
    grant->op = BARRGRANT;
    broadcast(slot);
    freemsg_unlock(slot);
}

/**
 * @brief broadcast -- broadcast msg to all hosts
 *
 * @param msg msg that will be broadcast
 * @param index index of msg in msg_buffer
 */
void broadcast(slot_t *slot) {
    int hosti;

    if (B_CAST == OFF) {
        for (hosti = 0; hosti < system_setting.hostc; hosti++) {
            slot->msg.topid = hosti;
            move_msg_to_outqueue(slot, &outqueue);
        }
    } else {
        bsendmsg(&slot->msg);
    }
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
        if ((msg->size + (wnptr->wtntc * (2*Intbytes + sizeof(unsigned char *)))) < Maxmsgsize) {
            for (wtnti = 0; wtnti < wnptr->wtntc; wtnti++) {
                appendmsg(msg, ltos(wnptr->wtnts[wtnti]), sizeof(unsigned char *));
                appendmsg(msg, ltos(wnptr->from[wtnti]), Intbytes);
                appendmsg(msg, ltos(wnptr->mig[wtnti]), Intbytes);
            }
            wnptr = wnptr->more;
        } else {
            full = 1;
        }
    }
    return (wnptr);
}

/************condv Part****************/

/**
 * @brief grantcondv -- append condv to CVGRANT msg and send it to toproc
 *
 * @param condv condition variable
 * @param toproc destination host
 */
void grantcondv(int condv, int toproc) {
    slot_t *slot;
    jia_msg_t *grant;

    slot = freemsg_lock(&msg_buffer);
    grant = &slot->msg;
    grant->op = CVGRANT;
    grant->frompid = system_setting.jia_pid;
    grant->topid = toproc;
    grant->size = 0;
    appendmsg(grant, ltos(condv), Intbytes);

    move_msg_to_outqueue(slot, &outqueue);
    freemsg_unlock(slot);
}

/************wtnt Part(record, send and wtnt stack)****************/

/**
 * @brief savewtnt -- save write notice about the page of the addr to ptr
 *
 * @param ptr lockstack[stackptr]'s wtnt_t pointer
 * @param addr the original address of cached page
 * @param frompid
 */
void savewtnt(wtnt_t *ptr, address_t addr, int mig, int frompid, int scope) {
    int wtnti;
    int exist;
    wtnt_t *wnptr;

    exist = 0;
    wnptr = ptr;

    /**
     * step 1:
     * check if there is already an address same to the addr in the WTNT list.
     */
    while ((exist == 0) && (wnptr != WNULL)) {
        wtnti = 0;
        while ((wtnti < wnptr->wtntc) && (((unsigned long)addr / Pagesize) != ((unsigned long)wnptr->wtnts[wtnti] / Pagesize))) {
            wtnti++;
        }

        // Traversing wtnt list to find addr
        if (wtnti >= wnptr->wtntc)
            wnptr = wnptr->more;
        else
            exist = 1;
    }

    /**
     * step 2:
     * existed: Change its frompid.
     * not existed: newwtnt() && record new addr
     */
    if (exist == 0) {
        wnptr = ptr;
        while (wnptr->wtntc >= Maxwtnts) {
            if (wnptr->more == WNULL)
                wnptr->more = newwtnt();
            wnptr = wnptr->more;
        }
        wnptr->wtnts[wnptr->wtntc] = addr;
        wnptr->from[wnptr->wtntc] = frompid;
        wnptr->scope[wnptr->wtntc] = scope;
        wnptr->mig[wnptr->wtntc] = mig;
        wnptr->wtntc++;
    } else {
        wnptr->from[wtnti] = Maxhosts;
        wnptr->scope[wtnti] = scope;
        wnptr->mig[wtnti] = mig;
    }
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
    for (datai = Intbytes; datai < req->size; datai += sizeof(unsigned char *))
        savewtnt(locks[lock].wtntp, (address_t)stol(req->data + datai), (int)stol(req->data + datai), req->frompid, locks[lock].scope);
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
    slot_t *slot;
    jia_msg_t *req;
    wtnt_t *wnptr; // write notice pointer

    log_info(3, "Enter sendwtnts!");

    slot = freemsg_lock(&msg_buffer);
    req = &(slot->msg);
    req->frompid = system_setting.jia_pid;
    req->topid = top.lockid % system_setting.hostc;
    req->size = 0;
    req->scope = (operation == BARR) ? locks[hidelock].myscope : locks[top.lockid].myscope;
    appendmsg(req, ltos(top.lockid), Intbytes);

    wnptr = top.wtntp;
    wnptr = appendstackwtnts(req, wnptr);

    /**
     * send wtnt(,Packing for delivery)
     * in the end send msg in which op==REL to finish this wtnt msg stream
     */
    while (wnptr != WNULL) {
        req->op = WTNT;
        move_msg_to_outqueue(slot, &outqueue);
        req->size = Intbytes;
        wnptr = appendstackwtnts(req, wnptr);
    }
    req->op = operation;
    move_msg_to_outqueue(slot, &outqueue);
    freemsg_unlock(slot);

    log_info(3, "Out of sendwtnts!");
}

/**
 * @brief appendstackwtnts -- append wtnt_t wtnts(i.e addr) to msg as many as
 * possible
 *
 * @param msg msg that include ptr->wtnts
 * @param ptr write notice pointer
 * @return wtnt_t*
 */
// wtnt_t *appendstackwtnts(jia_msg_t *msg, wtnt_t *ptr) {
//     int full; // flag indicate that msg full or not
//     wtnt_t *wnptr;

//     full = 0;
//     wnptr = ptr;
//     while ((wnptr != WNULL) && (full == 0)) {
//         if ((msg->size + (wnptr->wtntc * (sizeof(unsigned char *)))) < Maxmsgsize) {
//             appendmsg(msg, (unsigned char *)wnptr->wtnts, (wnptr->wtntc) * (sizeof(unsigned char *)));
//             wnptr = wnptr->more;
//         } else {
//             full = 1;
//         }
//     }
//     return (wnptr);
// }

wtnt_t *appendstackwtnts(jia_msg_t *msg, wtnt_t *ptr) {
    int wtnti;
    int full;
    wtnt_t *wnptr;

    full = 0;
    wnptr = ptr;
    while ((wnptr != WNULL) && (full == 0)) {
        if ((msg->size + (wnptr->wtntc * (Intbytes + sizeof(unsigned char *)))) < Maxmsgsize) {
            for (wtnti = 0; wtnti < wnptr->wtntc; wtnti++) {
                appendmsg(msg, ltos(wnptr->wtnts[wtnti]), sizeof(unsigned char *));
                appendmsg(msg, ltos(wnptr->mig[wtnti]), Intbytes);
            }
            wnptr = wnptr->more;
        } else {
            full = 1;
        }
    }
    return (wnptr);
}
