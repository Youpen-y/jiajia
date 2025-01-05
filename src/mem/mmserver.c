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
#include "mem.h"
#include "tools.h"
#include "utils.h"

/* jiajia */
extern int jia_pid;
extern host_t hosts[Maxhosts];
extern int hostc;

/* user */
extern jiahome_t home[Homepages + 1];    /* host owned page */
extern jiacache_t cache[Cachepages + 1]; /* host cached page */
extern jiapage_t page[Maxmempages];      /* global page space */

/* server */
volatile int getpwait;
volatile int diffwait;

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

    assert((req->op == DIFF) && (req->topid == jia_pid),
           "Incorrect DIFF Message!");

    datai = 0;
    while (datai < req->size) {
        // get cache addr
        paddr = s2l(req->data + datai); // consider use stol macro
        datai += sizeof(unsigned char *);
        wv = WVNULL;

        homei = homepage(paddr);

        /**
         * In the case of H_MIG==ON, homei may be the index of
         * the new home[] which has not be updated due to difference
         * of barrier arrival. In this case, homei==Homepages and
         * home[Homepages].wtnt==0
         */

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
            memcpy((address_t)(paddr + doffset), req->data + datai, dsize);
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

        if ((home[homei].wtnt & 1) != 1)
            memprotect((caddr_t)paddr, (size_t)Pagesize, (int)PROT_READ);

#ifdef DOSTAT
        STATOP(jiastat.mwdiffcnt++;)
#endif
    }

#ifdef DOSTAT
    STATOP(jiastat.dedifftime += get_usecs() - begin; jiastat.diffcnt++;)
#endif

    rep = newmsg();
    rep->op = DIFFGRANT;
    rep->frompid = jia_pid;
    rep->topid = req->frompid;
    rep->size = 0;

    asendmsg(rep);
    freemsg(rep);
}

/**
 * @brief diffgrantserver -- msg diffgrant server
 *
 * @param rep
 */
void diffgrantserver(jia_msg_t *rep) {
    assert((rep->op == DIFFGRANT) && (rep->size == 0),
           "Incorrect returned message!");

    diffwait--;
}

/**
 * @brief getpserver -- getp msg server
 *
 * @param req the request getp msg
 */
void getpserver(jia_msg_t *req) {
    address_t paddr;
    int homei;
    jia_msg_t *rep;

    assert((req->op == GETP) && (req->topid == jia_pid),
           "Incorrect GETP Message!");

    paddr = (address_t)stol(req->data); // getp message data is the page's addr
                                        /*
                                         VERBOSE_LOG(3, "getpage=0x%x from host %d\n",(unsigned long)
                                         paddr,req->frompid);
                                        */
    if ((H_MIG == ON) && (homehost(paddr) != jia_pid)) {
        /*This is a new home, the home[] data structure may
          not be updated, but the page has already been here
          the rdnt item of new home is set to 1 in migpage()*/
    } else {
        assert((homehost(paddr) == jia_pid),
               "This should have not been happened!");
        homei = homepage(paddr);

        if ((W_VEC == ON) && (home[homei].wvfull == 1)) {
            home[homei].wvfull = 0;
            newtwin(&(home[homei].twin));
            memcpy(home[homei].twin, home[homei].addr, Pagesize);
        }

        home[homei].rdnt = 1;
    }
    rep = newmsg();
    rep->op = GETPGRANT;
    rep->frompid = jia_pid;
    rep->topid = req->frompid;
    rep->temp = 0;
    rep->size = 0;
    // appendmsg(rep,req->data,Intbytes);  // reply msg data format
    // [req->data(4bytes), pagedata(4096bytes)], req->data is the page start
    // address
    appendmsg(rep, req->data, sizeof(unsigned char *)); // carry the addr

    if ((W_VEC == ON) && (req->temp == 1)) {
        int i;
        for (i = 0; i < Wvbits; i++) {
            if (((home[homei].wtvect[req->frompid]) & (((wtvect_t)1) << i)) !=
                0) {
                appendmsg(rep, paddr + i * Blocksize, Blocksize);
            }
        }
        rep->temp = home[homei].wtvect[req->frompid];
    } else {
        appendmsg(rep, paddr, Pagesize); // copy the page content to msg data
        rep->temp = WVFULL;
    }

    if (W_VEC == ON) {
        home[homei].wtvect[req->frompid] = WVNULL;
        /*
          VERBOSE_LOG(3, "0x%x\n",rep->temp);
        */
    }

    asendmsg(rep);
    freemsg(rep);
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
    int i;

    assert((rep->op == GETPGRANT), "Incorrect returned message!");

    wv = rep->temp;

    datai = 0;
    addr = (address_t)stol(rep->data + datai);
    // datai+=Intbytes;
    datai += sizeof(unsigned char *);

    if ((W_VEC == ON) && (wv != WVFULL)) {
        for (i = 0; i < Wvbits; i++) {
            if ((wv & (((wtvect_t)1) << i)) != 0) {
                memcpy(addr + i * Blocksize, rep->data + datai, Blocksize);
                datai += Blocksize;
            }
        }
    } else {
        VERBOSE_LOG(3, "addr is %p , rep->data+datai = %p\n", addr,
                    rep->data + datai);
        memcpy((unsigned char *)addr, rep->data + datai,
               Pagesize);
        VERBOSE_LOG(3, "I have copy the page from remote home to %p\n", addr);
    }

    getpwait = 0;
}