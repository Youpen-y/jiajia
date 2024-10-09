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

#ifndef NULL_LIB
#include "utils.h"
#include "tools.h"
#include "mem.h"

/* jiajia */
extern int jia_pid;
extern host_t hosts[Maxhosts];
extern int hostc;

jiahome_t home[Homepages + 1];    /* host owned page */
jiacache_t cache[Cachepages + 1]; /* host cached page */
jiapage_t page[Maxmempages];      /* global page space */
unsigned long globaladdr; /* [0, Maxmemsize)*/

/**
 * @brief jia_alloc3 -- allocates size bytes cyclically across all hosts, each
 * time block bytes
 *
 * @param size the sum of space that allocated across all hosts (page aligned)
 * @param block the size(page aligned) that allocated by every host every time
 * @param starthost specifies the host from which the allocation starts
 * @return unsigned long
 *
 * jia_alloc3 will allocate block(page aligned) every time from starthost to
 * other hosts until the sum equal size(page aligned)
 */
unsigned long jia_alloc3(int size, int block, int starthost) {
    int homepid;
    int mapsize;
    int allocsize;
    int originaddr;
    int homei, pagei, i;
    int protect;

    assert(((globaladdr + size) <= Maxmemsize),
           "Insufficient shared space! --> Max=0x%x Left=0x%lx Need=0x%x\n",
           Maxmemsize, Maxmemsize - globaladdr, size);

    originaddr = globaladdr;
    /* allocsize is the sum of size that will be allocated*/
    // ensure the alloc size is multiple of pagesize
    allocsize = SIZ2MULSIZ(size);
    /* mapsize is the size that will be allocated on every host evry time */
    // ensure the block size is multiple of pagesize
    mapsize = SIZ2MULSIZ(block);
    homepid = starthost;

    while (allocsize > 0) {
        // only when current host pid == homepid, use mmap alloc space
        if (jia_pid == homepid) {
            assert((hosts[homepid].homesize + mapsize) < (Homepages * Pagesize),
                   "Too many home pages");
            protect = (hostc == 1) ? PROT_READ | PROT_WRITE : PROT_READ;
            memmap((void *)(Startaddr + globaladdr), (size_t)mapsize, protect);

            // only when page on current host, set page's homei && homei's addr
            for (i = 0; i < mapsize; i += Pagesize) {
                pagei = (globaladdr + i) / Pagesize;
                homei = (hosts[homepid].homesize + i) / Pagesize;
                home[homei].addr = (address_t)(Startaddr + globaladdr + i);
                page[pagei].homei = homei;
            }
        }

        // page array is global, homepid must be set
        for (i = 0; i < mapsize; i += Pagesize) {
            pagei = (globaladdr + i) / Pagesize;
            page[pagei].homepid = homepid;
        }

        if (jia_pid == homepid) {
            VERBOSE_OUT(1, "Map 0x%x bytes in home %4d! globaladdr = 0x%lx\n",
                        mapsize, homepid, globaladdr);
        }

        hosts[homepid].homesize += mapsize;
        globaladdr += mapsize;
        allocsize -= mapsize;
        homepid = (homepid + 1) % hostc; // next host
    }
    return (Startaddr + originaddr);
}

/**
 * @brief jia_alloc3b --
 *
 * @param size
 * @param block
 * @param starthost
 * @return unsigned long
 */
unsigned long jia_alloc3b(int size, int *block, int starthost) {
    int homepid;
    int mapsize;
    int allocsize;
    int originaddr;
    int homei, pagei, i;
    int blocki;
    int protect;

    assert(((globaladdr + size) <= Maxmemsize),
           "Insufficient shared space! --> Max=0x%x Left=0x%lx Need=0x%x\n",
           Maxmemsize, Maxmemsize - globaladdr, size);

    blocki = 0;
    originaddr = globaladdr;
    allocsize =
        ((size % Pagesize) == 0) ? (size) : ((size / Pagesize + 1) * Pagesize);
    homepid = starthost;

    while (allocsize > 0) {
        mapsize = ((block[blocki] % Pagesize) == 0)
                      ? (block[blocki])
                      : ((block[blocki] / Pagesize + 1) * Pagesize);
        if (jia_pid == homepid) {
            assert((hosts[homepid].homesize + mapsize) < (Homepages * Pagesize),
                   "Too many home pages");

            protect = (hostc == 1) ? PROT_READ | PROT_WRITE : PROT_READ;
            memmap((void *)(Startaddr + globaladdr), (size_t)mapsize, protect);

            for (i = 0; i < mapsize; i += Pagesize) {
                pagei = (globaladdr + i) / Pagesize;
                homei = (hosts[homepid].homesize + i) / Pagesize;
                home[homei].addr = (address_t)(Startaddr + globaladdr + i);
                page[pagei].homei = homei;
            }
        }

        for (i = 0; i < mapsize; i += Pagesize) {
            pagei = (globaladdr + i) / Pagesize;
            page[pagei].homepid = homepid;
        }

#ifdef JIA_DEBUG
#endif
        printf("Map 0x%x bytes in home %4d! globaladdr = 0x%lx\n", mapsize,
               homepid, globaladdr);

        hosts[homepid].homesize += mapsize;
        globaladdr += mapsize;
        allocsize -= mapsize;
        homepid = (homepid + 1) % hostc;
        if (homepid == 0)
            blocki++;
    }

    return (Startaddr + originaddr);
}

unsigned long jia_alloc(int size) {
    static int starthost = -1;

    starthost = (starthost + 1) % hostc;
    return (jia_alloc3(size, size, starthost));
}

unsigned long jia_alloc1(int size) {
    return (jia_alloc3(size, size, 0));
}

unsigned long jia_alloc2(int size, int block) {
    return (jia_alloc3(size, block, 0));
}

/**
 * @brief jia_alloc2p -- two parameters alloc, alloc size(page aligned) on
 * host(proc is jiapid)
 *
 * @param size memory size that will be allocated(page aligned)
 * @param proc host(jia_pid == proc)
 * @return unsigned long
 */
unsigned long jia_alloc2p(int size, int proc) {
    return (jia_alloc3(size, size, proc));
}

#else  /* NULL_LIB */

unsigned long jia_alloc(int size) {
    return (unsigned long)valloc(size);
}
#endif /* NULL_LIB */