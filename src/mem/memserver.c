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
 *             Author: Weiwu Hu, Weisong Shi, Zhimin Tang              *
 * =================================================================== *
 *   This software is ported to SP2 by                                 *
 *                                                                     *
 *         M. Rasit Eskicioglu                                         *
 *         University of Alberta                                       *
 *         Dept. of Computing Science                                  *
 *         Edmonton, Alberta T6G 2H1 CANADA                            *
 * =================================================================== *
 **********************************************************************/

#include "comm.h"
#include "global.h"
#include "mem.h"
#include "msg.h"
#include "setting.h"
#include "stat.h"
#include "syn.h"
#include "tools.h"
#include "mem.h"
#include <pthread.h>
#include <stdatomic.h>
#include <string.h>

/* jiajia */
// extern int jia_pid;
// extern host_t hosts[Maxhosts];
// extern int hostc;

/* user */
extern jiahome_t home[Homepages];    /* host owned page */
extern jiacache_t cache[Cachepages]; /* host cached page */
extern jiapage_t page[Maxmempages];  /* global page space */

/* server */
_Atomic volatile int getpwait;
_Atomic volatile int diffwait;

/* tools */
extern int H_MIG, B_CAST, W_VEC;

#ifdef DOSTAT
extern jiastat_t jiastat;
extern int statflag;
#endif

/**
 * @brief diffserver -- msg diff server
 *s
 * @param req
 *
 * diff msg data : (| addr | totalsize | (start|count) | diffs) * n
 */
void diffserver(jia_msg_t *req) {
    int datai;
    int homei;
    unsigned long pstop, doffset, dsize;
    unsigned long paddr;
    jia_msg_t *rep;
    wtvect_t wv;

#ifdef DOSTAT
    register unsigned int begin = get_usecs();
#endif

    int jia_pid = system_setting.jia_pid;
    jia_assert((req->op == DIFF) && (req->topid == jia_pid),
               "Incorrect DIFF Message!");

    datai = 0;
    while (datai < req->size) {
        // get cache addr
        paddr = s2l(req->data + datai); // consider use stol macro
        datai += sizeof(unsigned char *);
        wv = WVNULL;

        homei = homepage((address_t)paddr);

        /**
         * In the case of H_MIG==ON, homei may be the index of
         * the new home[] which has not be updated due to difference
         * of barrier arrival. In this case, homei==Homepages and
         * home[Homepages].wtnt==0
         */

        // when home's wtnt==0, give RW permission for memcpy diff to homapage's
        // addr
        pthread_mutex_lock(&memory_mutex); // there, add lock protection to the memory
        if ((home[homei].wtnt & 1) != 1)
            memprotect((caddr_t)paddr, Pagesize, PROT_READ | PROT_WRITE);

        // pstop = (current diffmsg size + current offset - sizeof(addr)) =
        // current diffmsg's end offset
        pstop = bytestoi(req->data + datai) + datai - sizeof(unsigned char *);
        datai += Intbytes;
        while (datai < pstop) {
            // cal size && offset
            dsize = bytestoi(req->data + datai) & 0xffff;
            doffset = (bytestoi(req->data + datai) >> 16) & 0xffff;
            datai += Intbytes;

            // copy diffmsg to cache
            memcpy((address_t)(paddr + doffset), req->data + datai,
                   dsize); // there may be race condition with mainthread
            datai += dsize;

            // cal which diffunit should be modified
            if ((W_VEC == ON) && (dsize > 0)) {
                int i;
                for (i = doffset / Blocksize * Blocksize; i < (doffset + dsize);
                     i += Blocksize)
                    wv |= ((wtvect_t)1) << (i / Blocksize);
            }
        }


        if (W_VEC == ON)
            addwtvect(homei, wv, req->frompid);

        // after memcpy diff to homapage's addr, revoke write permission
        if ((home[homei].wtnt & 1) != 1)
            memprotect((caddr_t)paddr, (size_t)Pagesize, (int)PROT_READ);
        pthread_mutex_unlock(&memory_mutex); // memory protection end
#ifdef DOSTAT
        STATOP(jiastat.mwdiffcnt++;)
#endif
    }

#ifdef DOSTAT
    STATOP(jiastat.dedifftime += get_usecs() - begin; jiastat.diffcnt++;)
#endif

    // rep = newmsg();
    int index = freemsg_lock(&msg_buffer);
    rep = &msg_buffer.buffer[index].msg;
    rep->op = DIFFGRANT;
    rep->frompid = jia_pid;
    rep->topid = req->frompid;
    rep->size = 0;

    // asendmsg(rep);
    // freemsg(rep);
    move_msg_to_outqueue(&msg_buffer, index, &outqueue);
    freemsg_unlock(&msg_buffer, index);
}

/**
 * @brief diffgrantserver -- msg diffgrant server
 *
 * @param rep
 */
void diffgrantserver(jia_msg_t *rep) {
    jia_assert((rep->op == DIFFGRANT) && (rep->size == 0),
               "Incorrect returned message!");

    // diffwait--;
    atomic_fetch_sub(&diffwait, 1);
}

/**
 * @brief getpserver -- getp msg server
 *
 * @param req the request getp msg
 */
void getpserver(jia_msg_t *req) {
    address_t paddr;   // paddr is addr of the requested page
    address_t endaddr; // endaddr is the addr that checking boundary
    int homei;
    jia_msg_t *rep;
    int num = 0; // num is used to indicated that how much pages in this rep msg
    address_t addr[10] = {0}; // addr used to store that the addr of page that will be
                        // packed

    int jia_pid = system_setting.jia_pid;
    int prefetch_flag = prefetch_optimization.base.flag;
    int prefetch_pages = prefetch_optimization.prefetch_pages;
    int max_checking_pages = prefetch_optimization.max_checking_pages;

    jia_assert((req->op == GETP) && (req->topid == jia_pid),
               "Incorrect GETP Message!");

    paddr = (address_t)stol(req->data); // getp message data is the page's addr
    if ((H_MIG == ON) && (homehost(paddr) != jia_pid)) {
        /*This is a new home, the home[] data structure may
          not be updated, but the page has already been here
          the rdnt item of new home is set to 1 in migpage()*/
    } else {

        jia_assert((homehost(paddr) == jia_pid),
                   "This should have not been happened!");

        homei = homepage(paddr);
        if ((W_VEC == ON) && (home[homei].wvfull == 1)) {
            pthread_mutex_lock(&memory_mutex);  // there, add protection to memory access
            home[homei].wvfull = 0;
            newtwin(&(home[homei].twin));
            memcpy(home[homei].twin, home[homei].addr, Pagesize);
            pthread_mutex_unlock(&memory_mutex);  // there, memory protection ends
        }

        num = 1;
        addr[0] = paddr;
        /* somebody will have a valid copy*/
        home[homei].rdnt = 1;

        if (prefetch_flag && prefetch_pages) {
            paddr += Pagesize;
            endaddr = paddr + max_checking_pages * Pagesize;

            while (paddr < endaddr && num < max_checking_pages + 1) {
                // checking the next page's home
                if (homehost(paddr) == jia_pid) {
                    homei = homepage(paddr);

                    // some host will have a valid copy of this page
                    home[homei].rdnt = 1;
                    addr[num] = paddr; // use num as a counter
                    num++;
                    if (num == prefetch_optimization.prefetch_pages + 1) {
                        break;
                    }
                }
                // next page
                paddr += Pagesize;
            }
        }
    }

    int index = freemsg_lock(&msg_buffer);
    rep = &msg_buffer.buffer[index].msg;
    rep->op = GETPGRANT;
    rep->frompid = jia_pid;
    rep->topid = req->frompid;
    rep->temp = 0;
    rep->size = 0;

    // GETPGRANT msg format { header(32bytes) |  num (4bytes) | addr1(8bytes) |
    // pagedata1(4096bytes) | addr2 | pagedata2 | ... }
    appendmsg(rep, (unsigned char *)&num,
              sizeof(int)); // carry total num of pages(requested page +
                            // prefetched apges)

    appendmsg(rep, req->data,
              sizeof(unsigned char *)); // carry the addr of requested page
    int occupysize = 0;
    pthread_mutex_lock(&memory_mutex);
    if ((W_VEC == ON) && (req->temp == 1)) {
        int i;
        for (i = 0; i < Wvbits; i++) {
            if (((home[homei].wtvect[req->frompid]) & (((wtvect_t)1) << i)) !=
                0) {
                occupysize += Blocksize;
                appendmsg(rep, addr[0] + i * Blocksize, Blocksize);
            }
        }
        rep->temp = home[homei].wtvect[req->frompid];
        rep->size += (Pagesize - occupysize); // occupy a whole of page
    } else {
        rep->temp = WVFULL;
        appendmsg(rep, addr[0], Pagesize); // copy the page content to msg data
    }

    for (int i = 1; i < num; i++) {
        appendmsg(rep, (unsigned char *)&addr[i], sizeof(unsigned char *));
        appendmsg(rep, addr[i], Pagesize);
    }
    pthread_mutex_unlock(&memory_mutex);
    move_msg_to_outqueue(&msg_buffer, index, &outqueue);
    freemsg_unlock(&msg_buffer, index);
}

/**
 * @brief getpgrantserver -- getpgrant msg server
 *
 * @param rep the reply msg getpgrant
 */
void getpgrantserver(jia_msg_t *rep) {
    address_t addr;
    unsigned int datai;
    unsigned long wv;
    int i, num, cachei;

    jia_assert((rep->op == GETPGRANT), "Incorrect returned message!");

    wv = rep->temp; // get the write vector bit map

    datai = 0;
    num = bytestoi(rep->data);  // get the num of pages that in reply msg
    datai += sizeof(int);

    addr = (address_t)stol(rep->data + datai);
    datai += sizeof(unsigned char *);

    int occupysize = 0;
    pthread_mutex_lock(&memory_mutex);
    if ((W_VEC == ON) && (wv != WVFULL)) {
        for (i = 0; i < Wvbits; i++) {
            if ((wv & (((wtvect_t)1) << i)) != 0) {
                memcpy(addr + i * Blocksize, rep->data + datai, Blocksize);
                datai += Blocksize;
                occupysize += Blocksize;
            }
        }
        datai += (Pagesize - occupysize);
    } else {
        // addr: current host's cache addr(dst addr)
        // srcdata: other host's homepage data(packing in rep)
        unsigned char *srcdata = rep->data + datai;
        log_info(3, "addr is %p , rep->data+datai = %p", addr,
                 rep->data + datai);
        memcpy((unsigned char *)addr, srcdata, Pagesize);
        log_info(3, "I have copy the page from remote home to %p", addr);
        datai += Pagesize;
    }

    // handle prefetched pages
    for (int i = 1; i < num; i++) {
        addr = (address_t)stol(rep->data + datai);
        datai += sizeof(unsigned char *);
        cachei = cachepage(addr);

        if (cachei < Cachepages) {
            // do nothing
        } else {
            cachei = findposition(addr);
            if (cachei != -1) {
                memmap(addr, Pagesize, PROT_READ | PROT_WRITE);
                memcpy(addr, (unsigned char *)(rep->data + datai), Pagesize);
                
                cache[cachei].addr = addr;
                cache[cachei].state = RO;
                memprotect((caddr_t)addr, (size_t)Pagesize, PROT_READ);
            }
        }
        datai += Pagesize;
    }
    pthread_mutex_unlock(&memory_mutex);

#ifdef DOSTAT
    jiastat.prefetchcnt += (num-1);
#endif

    atomic_store(&getpwait, 0);
}
