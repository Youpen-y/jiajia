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
#include "jia.h"
#include "mem.h"
#include "tools.h"
#include "setting.h"
#include "stat.h"

/* mem */
extern jiapage_t page[Maxmempages];
extern jiacache_t cache[Cachepages];
extern jiahome_t home[Homepages];
extern void setwtvect(int homei, wtvect_t wv);
extern void addwtvect(int homei, wtvect_t wv, int from);
extern void flushpage(int cachei);

/* jiajia */
// extern host_t hosts[Maxhosts];
extern int H_MIG, W_VEC;

/* syn */
extern jiastack_t lockstack[Maxstacksize]; 
extern int stackptr;

#ifdef DOSTAT
extern int statflag;
extern jiastat_t jiastat;
#endif

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
            cachei = page[(addr - system_setting.global_start_addr) / Pagesize].cachei;
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
                page[((unsigned long)home[homei].addr - system_setting.global_start_addr) / Pagesize]
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
    system_setting.hosts[system_setting.jia_pid].homesize = (i - 1) * Pagesize;
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

    pagei = (addr - system_setting.global_start_addr) / Pagesize;
    /*
     VERBOSE_LOG(3, "Mig page 0x%x from host %d to %d\n",pagei,frompid,topid);
    */

    if (topid == system_setting.jia_pid) { /*New Home*/
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
            jia_assert(0, "Home exceed in home migration");
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
            jia_assert(0, "This should not have happened---MIG");
        }
#ifdef DOSTAT
        if (statflag == 1) {
            jiastat.migincnt++;
        }
#endif
    } else if (frompid == system_setting.jia_pid) { /*Old Home*/
        homei = homepage((address_t)addr);
        jia_assert((unsigned long)home[homei].addr == addr, "MIG ERROR");

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