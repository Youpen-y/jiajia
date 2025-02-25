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
 **********************************************************************/

#include "comm.h"
#include "mem.h"
#include "msg.h"
#include "setting.h"
#include "stat.h"
#include "syn.h"
#include "tools.h"
#include <stdatomic.h>
#include <execinfo.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* user */
extern jiahome_t home[Homepages];    /* host owned page */
extern jiacache_t cache[Cachepages]; /* host cached page */
extern jiapage_t page[Maxmempages];  /* global page space */
extern unsigned long globaladdr;

/* server */
extern _Atomic volatile int getpwait;
extern _Atomic volatile int diffwait;

/* tools */
extern int H_MIG, B_CAST, W_VEC;

/* syn */
extern jiastack_t lockstack[Maxstacksize]; // lock stack
extern int stackptr;

#ifdef DOSTAT
extern jiastat_t jiastat;
extern int statflag;
#endif

slot_t *diffmsg[Maxhosts]; /* store every host's diff msgs */
long jiamapfd; /* file descriptor of the file that mapped to process's virtual
                  address space */
int repcnt[Setnum]; /* record the last replacement index of every set */

static void savediff(int cachei);

/**
 * @brief xor -- get the index of the first page of the set based on the addr,
 * setnum group connection
 *
 * @param addr address of a byte in one page
 * @return int -  the first cache index of the page's corresponding setnum in
 * the cache
 *
 */
static inline int xor (address_t addr) {
    return ((((unsigned long)(addr - system_setting.global_start_addr) / Pagesize) % Setnum) *
            Setpages);
}

/**
 * @brief flushpage -- flush the cached page(reset the cache's info);
 * remove the cache relation with its original page. if the cache's state is RW,
 * free its twin
 *
 * @param cachei page index in cache
 */
void flushpage(int cachei) {
    memunmap((void *)cache[cachei].addr, Pagesize);

    cachepage(cache[cachei].addr) =
        Cachepages; // normal cachi range: [0, Cachepages)

    if (cache[cachei].state == RW) { // cache's state equals RW means that there
                                     // is a twin(copy) for the cached page.
        freetwin(&(cache[cachei].twin));
    }
    cache[cachei].state = UNMAP;
    cache[cachei].wtnt = 0;
    cache[cachei].addr = 0;
}

/**
 * @brief getpage -- according to addr, get page from remote host (page's home)
 *
 * @param addr page global address
 * @param flag indicate read(0) or write(1) request
 */
void getpage(address_t addr, int flag) {
    int homeid;
    jia_msg_t *req;

    homeid = homehost(addr);
    jia_assert((homeid != system_setting.jia_pid),
               "This should not have happened 2!");

    slot_t* slot = freemsg_lock(&msg_buffer);
    req = &slot->msg;

    req->op = GETP;
    req->frompid = system_setting.jia_pid;
    req->topid = homeid;
    req->temp = flag; /*0:read request, 1:write request*/
    req->size = 0;
    // append address of request page to data[0..7]
    appendmsg(req, ltos(addr), sizeof(unsigned char *));
    atomic_store(&getpwait, 1);
    move_msg_to_outqueue(slot, &outqueue);
    freemsg_unlock(slot);

#ifdef DOSTAT
    if (statflag == 1) {
        jiastat.getpcnt++;
    }
#endif
}

// TODO: implement LRU replacement
/**
 * @brief replacei - return the cache index that will be replaced according
 * to different replacement scheme
 *
 * @param cachei: index of cache
 * @return int - the index of a cache item that that will be replaced (in
 * one set)
 */
int replacei(int cachei) {
    int seti; // set index

    if (REPSCHEME == 0) // replace scheme equals to zero, random replacement
        return ((random() >> 8) % Setpages);
    else { // circular replacement in corresponding set
        seti = cachei / Setpages;
        repcnt[seti] = (repcnt[seti] + 1) % Setpages;
        return (repcnt[seti]);
    }
}


// int replacei(int cachei, repscheme_t flag) {
//     int seti, repi;
//     switch (flag) {
//         case RANDOM:
//             repi = ((random() >> 8) % Setpages);
//             break;
//         case CIRCULAR:
//             seti = cachei / Setpages;
//             repcnt[seti] = (repcnt[seti] + 1) % Setpages;
//             repi = repcnt[seti];
//             break;
//         case LRU:

//             break;
//     }
//     return repi;
// }




/**
 * @brief findposition -- find an available cache slot in cache
 *
 * @param addr addr of a byte in one page
 * @return int the index of
 */
int findposition(address_t addr) {
    int cachei; /*index of cache*/
    int seti;   /*index in a set*/
    int invi;   /*invalid index*/
    int i;

    cachei = xor(addr);
    seti = replacei(cachei);
    invi = -1;

    /**
     * find a cached page whose state is UNMAP (or INV, use invi to record it)
     * or the last page in this set
     * UNMAP is the highest priority， second is INV
     */
    for (i = 0; (cache[cachei + seti].state != UNMAP) && (i < Setpages); i++) {
        if ((invi == (-1)) && (cache[cachei + seti].state == INV))
            invi = seti;
        seti = (seti + 1) % Setpages; // next index in this set
    }

    // if there is no UNMAP cache, used cache in INV state
    if ((cache[cachei + seti].state != UNMAP) && (invi != (-1))) {
        seti = invi;
    }

    // cache priorty sequence: UNMAP > INV > RO > RW

    /**
     * INV||RO : flush
     * RW : save&&flush
     * UNMAP : no op
     */
    if ((cache[cachei + seti].state == INV) ||
        (cache[cachei + seti].state == RO)) {
        flushpage(cachei + seti);
#ifdef DOSTAT
        if (statflag == 1) {
            if (cache[cachei + seti].state == RO)
                jiastat.repROcnt++;
        }
#endif
    } else if (cache[cachei + seti].state == RW) {
        return -1;
        savepage(cachei + seti);
        senddiffs();
        while (diffwait)
            ;
        flushpage(cachei + seti);
#ifdef DOSTAT
        if (statflag == 1) {
            jiastat.repRWcnt++;
        }
#endif
    }
    cachepage(addr) = (unsigned short)(cachei + seti);
    return (cachei + seti);
}

#ifdef SOLARIS
void (int signo, siginfo_t *sip, ucontext_t *uap)
#endif

#ifdef LINUX
void sigsegv_handler(int signo, siginfo_t *sip, void *context)
#endif
{
    address_t faultaddr;
    int writefault;
    int cachei, homei;

    sigset_t set;

#ifdef DOSTAT
    register unsigned int begin = get_usecs();
    if (statflag == 1) {
        jiastat.kernelflag = 2;
    }
#endif

    sigemptyset(&set);
    sigaddset(&set, SIGSEGV);
    sigprocmask(SIG_BLOCK, &set, NULL);

#ifdef SOLARIS
    faultaddr = (address_t)sip->si_addr;
    faultaddr = (address_t)((unsigned long)faultaddr / Pagesize * Pagesize);
    writefault = (int)(*(unsigned *)uap->uc_mcontext.gregs[REG_PC] & (1 << 21));
#endif

#ifdef LINUX
    faultaddr = (address_t)sip->si_addr;
    faultaddr =
        (address_t)((unsigned long long)faultaddr / Pagesize * Pagesize);
    /**
     * si_code: 1 means that address not mapped to object
     * si_code: 2 means that invalid permissions for mapped object
     */
    writefault = sip->si_code & 2;
#endif

    log_info(3, "Enter sigsegv_handler");
    log_info(3,
             "Shared memory range from %p to %p!, faultaddr=%p(%d), "
             "writefault=%d",
             (void *)system_setting.global_start_addr,
             (void *)(system_setting.global_start_addr + globaladdr), faultaddr, homehost(faultaddr),
             writefault);

    log_info(3,
             "\nsig info structure siginfo_t"
             "\n\tsignal err : %d \n"
             "\tsignal code: %d \n"
             "\t    si_addr: %p",
             sip->si_errno, sip->si_code, sip->si_addr);

    jia_assert(
        (((unsigned long long)faultaddr <
          (system_setting.global_start_addr + globaladdr)) &&
         ((unsigned long long)faultaddr >= system_setting.global_start_addr)),
        "Access shared memory out of range from %p to %p!, "
        "faultaddr=%p, writefault=%d",
        (void *)system_setting.global_start_addr,
        (void *)(system_setting.global_start_addr + globaladdr),
        (void *)faultaddr, writefault);

    // page's home is current host (si_code = 2)
    if (homehost(faultaddr) == system_setting.jia_pid) {
        // grant write right beyond read right
        memprotect((caddr_t)faultaddr, Pagesize, PROT_READ | PROT_WRITE);
        homei = homepage(faultaddr);
        home[homei].wtnt |= 3;
        if ((W_VEC == ON) && (home[homei].wvfull == 0)) {
            newtwin(&(home[homei].twin));
            memcpy(home[homei].twin, home[homei].addr, Pagesize);
        }
#ifdef DOSTAT
        STATOP(jiastat.segvLtime += get_usecs() - begin; jiastat.kernelflag = 0;
               jiastat.segvLcnt++;)
#endif
    } else { // page's home is not current host (si_code = 1, )

        // page on other host, page must be get before other operations
        writefault = (writefault == 0) ? 0 : 1;
        cachei = cachepage(faultaddr);

        /**
         * cachei != Cachepages: page's cache has exist
         * cachei == Cachepages: should be mmapped
         * note: first prsigsegv_handlerotect it as writable, and then make changes based on
         * writefault later. first writable permission is for getpgrantserver's
         * copy(to faultaddr)
         */

        if (cachei < Cachepages) {
            memprotect((caddr_t)faultaddr, Pagesize, PROT_READ | PROT_WRITE);
            if (!((writefault) && (cache[cachei].state == RO))) {
                getpage(faultaddr, 1);
            }
        } else {
            cachei = findposition(faultaddr);
            memmap((caddr_t)faultaddr, Pagesize, PROT_READ | PROT_WRITE);
            getpage(faultaddr, 0);
        }

        /**
         * writefault == 0(not mapped): getpage without RW permission(only RO)
         * writefault == 1(not permitted): give faultaddr RW permission to write
         */
        if (writefault) {
            cache[cachei].addr = faultaddr;
            cache[cachei].state = RW;
            cache[cachei].wtnt = 1;
            // new twin to store origin data for compare and senddiffs later
            newtwin(&(cache[cachei].twin));
            while (atomic_load(&getpwait))  // after send GETP msg, main thread stuck here until get GETPGRANT
                ;
            memcpy(cache[cachei].twin, faultaddr, Pagesize);
        } else {
            cache[cachei].addr = faultaddr;
            cache[cachei].state = RO;
            while (atomic_load(&getpwait))  // or main thread stuck here until get GETPGRANT
                ;
            memprotect((caddr_t)faultaddr, (size_t)Pagesize, PROT_READ);
        }

#ifdef DOSTAT
        if (statflag == 1) {
            jiastat.segvRcnt++;
            jiastat.segvRtime += get_usecs() - begin;
            jiastat.kernelflag = 0;
        }
#endif
    }
    log_info(3, "Out of sigsegv_handler\n");
}

/**
 * @brief encodediff() -- encode the diff of cache page(cachei) and its twin to
 * the paramater diff
 *
 * @param cachei cache page index
 * @param diff address that used to save difference
 * @return the total size of bytes encoded in diff
 *
 * diff[]:
 * | cache page addr (8bytes) | size of all elements in diff[] (4bytes) |
 * (start,size) 4bytes | cnt bytes different data |
 */
int encodediff(int cachei, unsigned char *diff) {
    int size = 0;
    int bytei = 0;
    int cnt = 0;
    int start;
    unsigned header;

#ifdef DOSTAT
    register unsigned int begin = get_usecs();
#endif

    // step 1: encode the cache page addr first
    memcpy(diff + size, ltos(cache[cachei].addr), sizeof(unsigned char *));
    size += sizeof(unsigned char *);
    size += Intbytes; /* leave space for size */

    // here we got the difference between cache page and its twin
    // check how much diffunit is different in one Page (bytei is the index)
    while (bytei < Pagesize) {
        // find the start byte index of the diff
        for (; (bytei < Pagesize) &&
               (memcmp(cache[cachei].addr + bytei, cache[cachei].twin + bytei,
                       Diffunit) == 0);
             bytei += Diffunit)
            ;

        if (bytei < Pagesize) {
            cnt = 0;
            start = bytei; // record the start byte index of the diff

            // how much diffunit is different
            for (; (bytei < Pagesize) &&
                   (memcmp(cache[cachei].addr + bytei,
                           cache[cachei].twin + bytei, Diffunit) != 0);
                 bytei += Diffunit)
                cnt += Diffunit;

            // step 2: encode the header
            // header is composed of start and cnt(diff size)
            header = ((start & 0xffff) << 16) | (cnt & 0xffff);
            memcpy(diff + size, ltos(header),
                   Intbytes); // step 2: encode the header
            size += Intbytes;

            // step 3: encode cnt different bytes
            memcpy(diff + size, cache[cachei].addr + start, cnt);
            size += cnt;
        }
    }
    memcpy(diff + sizeof(unsigned char *), ltos(size),
           Intbytes); // step 4: fill the size

#ifdef DOSTAT
    if (statflag == 1) {
        jiastat.endifftime += get_usecs() - begin;
    }
#endif
    return (size);
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
 * @brief savediff() -- save the difference between cached page(cachei) and its
 * twin
 *
 * @param cachei cache page index
 *
 * If the diffsize + diffmsg[hosti] > Maxmsgsize, asendmsg; other, append the
 * diff to the diffmsg[hosti] hosti is the cached page's home host
 */
void savediff(int cachei) {
    unsigned char diff[Maxmsgsize]; // msg data array to store diff
    int diffsize;
    int hosti;
    slot_t* slot;
    jia_msg_t* tmp;

    // according to cachei addr get the page's home host index
    hosti = homehost(cache[cachei].addr);
    if (diffmsg[hosti] == DIFFNULL) { // hosti host's diffmsg is NULL
        slot = freemsg_lock(&msg_buffer);
        diffmsg[hosti] = slot;
        tmp = &slot->msg;
        tmp->op = DIFF;
        tmp->frompid = system_setting.jia_pid;
        tmp->topid = hosti;
        tmp->size = 0;
    }
    
    // encoded the difference data between cachei page and
    // its twin into diff [] and return size
    diffsize = encodediff(cachei, diff); 
    if ((tmp->size + diffsize) > Maxmsgsize) {
        atomic_fetch_add(&diffwait, 1);
        log_info(4, "diffwait: %d", diffwait);
        move_msg_to_outqueue(slot, &outqueue);
        tmp->size = 0;
        appendmsg(tmp, diff, diffsize);
        while (diffwait)
            ;
    } else {
        appendmsg(tmp, diff, diffsize);
    }
}

/**
 * @brief senddiffs() -- send msg in diffmsg[hosti] to correponding hosti host
 *
 */
void senddiffs() {
    int hosti;
    for (hosti = 0; hosti < system_setting.hostc; hosti++) {
        if (diffmsg[hosti] != DIFFNULL) { // hosti's diff msgs is non-NULL
            if (diffmsg[hosti]->msg.size > 0) { // diff data size > 0
                atomic_fetch_add(&diffwait, 1);
                log_info(4, "diffwait: %d", diffwait);
                move_msg_to_outqueue(diffmsg[hosti], &outqueue);
                freemsg_unlock(diffmsg[hosti]);
            }
            diffmsg[hosti] = DIFFNULL;
        }
    }

    // while (atomic_load(&diffwait))
    //     ; // diffwait is detected after senddiffs() called
}