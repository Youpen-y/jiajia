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

#include "mem.h"
#include "comm.h"
#include "tools.h"
#include "utils.h"
#include "setting.h"
#include "stat.h"

/* user */
extern jiahome_t home[Homepages];       /* host owned page */
extern jiacache_t cache[Cachepages];     /* host cached page */
extern jiapage_t page[Maxmempages];      /* global page space */
extern unsigned long globaladdr;         /* [0, Maxmemsize)*/

/* server */
extern volatile int getpwait;
extern volatile int diffwait;

/* mmsync */
extern jia_msg_t *diffmsg[Maxhosts]; /* store every host's diff msgs */
extern long jiamapfd; /* file descriptor of the file that mapped to process's virtual
                  address space */
extern int repcnt[Setnum]; /* record the last replacement index of every set */


int homehost(address_t addr){
    return page[((unsigned long)(addr)-system_setting.global_start_addr) / Pagesize].homepid;
}

unsigned int homepage(address_t addr){
    return page[((unsigned long)(addr)-system_setting.global_start_addr) / Pagesize].homei;
}

unsigned int cachepage(address_t addr){
    return page[((unsigned long)(addr)-system_setting.global_start_addr) / Pagesize].cachei;
}


/**
 * @brief initmem - initialize memory setting (wait for SIGSEGV signal)
 *
 * step1: initialize diff msg for every host and some condition
 * variables(diffwait, getpwait)
 *
 * step2: initialize the homesize of every host
 *
 * step3: initialize every home page's attributes
 *
 * step4: initialize every page's attributes
 *
 * step5: initialize cache page's attributes
 *
 * step6: register sigsegv handler
 */
void initmem() {
    int i, j;

    for (i = 0; i < Maxhosts; i++) { // step1
        diffmsg[i] = DIFFNULL;
    }
    diffwait = 0;
    getpwait = 0;

    for (i = 0; i <= system_setting.hostc; i++) { // step2: set every host's homesize to 0
        system_setting.hosts[i].homesize = 0;
    }

    for (i = 0; i < Homepages; i++) {
        home[i].wtnt = 0;
        home[i].rdnt = 0;
        home[i].addr = (address_t)0;
        home[i].twin = NULL;
    }

    for (i = 0; i < Maxmempages; i++) {
        page[i].cachei = (unsigned short)Cachepages;
        page[i].homei = (unsigned short)Homepages;
        page[i].homepid = (unsigned short)Maxhosts;
    }

    for (i = 0; i < Cachepages; i++) {
        cache[i].state = UNMAP; /* initial cached page state is UNMAP */
        cache[i].addr = 0;
        cache[i].twin = NULL;
        cache[i].wtnt = 0;
    }

    globaladdr = 0;

#if defined SOLARIS || defined IRIX62
    jiamapfd = open("/dev/zero", O_RDWR, 0);

    {
        struct sigaction act;

        // act.sa_handler = (void_func_handler)sigsegv_handler;
        act.sa_sigaction = (void_func_handler)sigsegv_handler;
        sigemptyset(&act.sa_mask);
        act.sa_flags = SA_SIGINFO;
        if (sigaction(SIGSEGV, &act, NULL))
            local_assert(0, "segv sigaction problem");
    }
#endif

#ifdef LINUX
    jiamapfd = open("/dev/zero", O_RDWR,
                    0); /* file descriptor refer to the open file */
    { /* reference to a non-home page causes the delivery of a SIGSEGV signal,
      the SIGSEGV handler then maps the fault page to the global address of the
      page in local address space */
        struct sigaction act;
        act.sa_handler = (void_func_handler)sigsegv_handler;
        sigemptyset(&act.sa_mask);
        // act.sa_flags = SA_NOMASK;
        // act.sa_flags = SA_NODEFER;  /* SA_NOMASK is obsolete */
        act.sa_flags = SA_NODEFER | SA_SIGINFO;
        if (sigaction(SIGSEGV, &act, NULL))
            local_assert(0, "segv sigaction problem");
    }
#endif

#ifdef AIX41
    {
        struct sigvec vec;

        vec.sv_handler = (void_func_handler)sigsegv_handler;
        vec.sv_flags = SV_INTERRUPT;
        sigvec(SIGSEGV, &vec, NULL);
    }
#endif /* SOLARIS */

    for (i = 0; i < Setnum; i++) { // TODO: consider multiple sets
        repcnt[i] = 0;
    }
    srand(1);
}

/**
 * @brief set [addr, addr+len-1] memory's protection to prot
 *
 * @param addr start address of memory
 * @param len memory length
 * @param prot protection flag
 */
void memprotect(void *addr, size_t len, int prot) {
    int protyes;

    protyes = mprotect(addr, len, prot);
    jia_assert(!protyes, "mprotect failed! addr=0x%lx, errno=%d",
           (unsigned long)addr, errno);
}

/**
 * @brief memmap - map jiamapfd file to process's address space [addr,
 * addr+len-1]
 *
 * @param addr start address of the mapped memory
 * @param len length of mapped memory
 * @param prot mapped memory's protection level
 * (PROT_READ/PROT_WRITE/PROT_EXEC/PROT_NONE)
 */
void memmap(void *addr, size_t len, int prot) {
    void *mapad;

#if defined SOLARIS || defined IRIX62 || defined LINUX
    // map file descriptor jiamapfd refered file to process virtual memory
    // [addr, addr+length-1] with protection level prot
    mapad = mmap(addr, len, prot, MAP_PRIVATE | MAP_FIXED, jiamapfd, 0);
#endif
#ifdef AIX41
    mapad =
        mmap(addr, len, prot, MAP_PRIVATE | MAP_FIXED | MAP_ANONYMOUS, -1, 0);
#endif

    if (mapad != addr) {
        jia_assert(0, "mmap failed! addr=0x%lx, errno=%d", (unsigned long)(addr),
               errno);
    }
}

/**
 * @brief memunmap - unmap memory, remove the mapped memory from process virtual
 * address space
 *
 * @param addr start address of virtual mapped memory that should be cancelled
 * ()
 * @param len length of mapped memory that need to removed
 */
void memunmap(void *addr, size_t len) {
    int unmapyes;

    unmapyes = munmap(addr, len);
    if (unmapyes != 0) {
        jia_assert(0, "munmap failed! addr=0x%lx, errno=%d", (unsigned long)addr,
               errno);
    }
}

/**
 * @brief addwtvect --
 *
 * @param homei
 * @param wv
 * @param from
 */
void addwtvect(int homei, wtvect_t wv, int from) {
    int i;

    home[homei].wvfull = 1;
    for (i = 0; i < system_setting.hostc; i++) {
        if (i != from)
            home[homei].wtvect[i] |= wv;

        if (home[homei].wtvect[i] != WVFULL)
            home[homei].wvfull = 0;
    }
}

/**
 * @brief setwtvect --
 *
 * @param homei
 * @param wv
 * @param from
 */
void setwtvect(int homei, wtvect_t wv) {
    int i;

    home[homei].wvfull = 1;
    for (i = 0; i < system_setting.hostc; i++) {
        home[homei].wtvect[i] = wv;
        if (home[homei].wtvect[i] != WVFULL)
            home[homei].wvfull = 0;
    }
}