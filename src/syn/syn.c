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

#include "syn.h"
#include "comm.h"
#include "global.h"
#include "init.h"
#include "jia.h"
#include "mem.h"
#include "tools.h"
#include "utils.h"
#include "setting.h"
#include "stat.h"
#include <stdatomic.h>

/* jiajia */
// extern int jia_pid;
// extern host_t hosts[Maxhosts];
// extern int hostc;

/* mem */
extern jiapage_t page[Maxmempages];
extern jiacache_t cache[Cachepages];
extern jiahome_t home[Homepages];
extern _Atomic volatile int diffwait;

/* tools */
extern int H_MIG, AD_WD, W_VEC;

#ifdef DOSTAT
extern int statflag;
extern jiastat_t jiastat;
#endif

/* syn */
// lock array, according to the hosts allocate lock(eg.
// host0: 0, 2,... 62; host1 = 1, 3, ..., 63)
jialock_t locks[Maxlocks + 1];
jiastack_t lockstack[Maxstacksize]; // lock stack

int stackptr;
_Atomic volatile int waitwait, acqwait, barrwait, cvwait;
volatile int noclearlocks;
volatile int waitcounter;
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
        if ((i % system_setting.hostc) == system_setting.jia_pid)
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
 * @brief endinterval -- end an interval, save the changes and send them to
 * their home page
 *
 * @param synop syn operation ACQ/BARR
 */
void endinterval(int synop) {
    // register advise compiler store these variables into registers
    register int cachei;
    register int pagei;
    register int hpages;
    log_info(3, "Enter endinterval!");

    /*/ step 1: save all cache diff */
    for (cachei = 0; cachei < Cachepages; cachei++) {
        // cachepage's wtnt == 1 means RW permission
        if (cache[cachei].wtnt == 1) {
            savepage(cachei);
        }
    }
    senddiffs();

    // step 2: save all home page wtnts
    hpages = system_setting.hosts[system_setting.jia_pid].homesize / Pagesize; // page number of jia_pid host
    for (pagei = 0; pagei < hpages; pagei++) {
        /** home[pagei].wtnt & 1: home host has written this homepage */
        if ((home[pagei].wtnt & 1) != 0) { 
            /** home[pagei].rdnt != 0: remote host has valid copy of this homepage(cachepage) */
            if (home[pagei].rdnt != 0) {
                // remote host && home host has different copy, so we will savewtnts to record it
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
                addwtvect(pagei, wv, system_setting.jia_pid);
            }

        } /*if*/
    }     /*for*/
    while (atomic_load(&diffwait))
        ; // wait all diffs were handled
    log_info(3, "Out of endinterval!");
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
        // get the addr
        addr = (address_t)stol(req->data + datai); 
        if (H_MIG == ON) {
            migtag = ((unsigned long)addr) % Pagesize;
            addr = (address_t)(((unsigned long)addr / Pagesize) * Pagesize);
        }
        datai += sizeof(unsigned char *);

        // TODO: Barrier or Lock?
        if (lock == hidelock) { /*Barrier*/
            from = (int)stol(req->data + datai);
            datai += Intbytes;
        } else { /*Lock*/
            from = Maxhosts;
        }

        // invalidate all pages that are not on this host(cache)
        if ((from != system_setting.jia_pid) && (homehost(addr) != system_setting.jia_pid)) {
            cachei = (int)cachepage(addr);
            if (cachei < Cachepages) {
                if (cache[cachei].state != INV) {
                    if (cache[cachei].state == RW)
                        freetwin(&(cache[cachei].twin));
                    cache[cachei].wtnt = 0;
                    cache[cachei].state = INV;
                    memprotect((caddr_t)cache[cachei].addr, Pagesize,
                               PROT_NONE);
#ifdef DOSTAT
                    STATOP(jiastat.invcnt++;)
#endif
                }
            }
        }

        if ((H_MIG == ON) && (lock == hidelock) && (from != Maxhosts) &&
            (migtag != 0)) {
            migpage((unsigned long)addr, homehost(addr), from);
        }

        if ((AD_WD == ON) && (lock == hidelock) &&
            (homehost(addr) == system_setting.jia_pid) && (from != system_setting.jia_pid)) {
            homei = homepage(addr);
            home[homei].wtnt |= 4;
        }

    } /*while*/
}
