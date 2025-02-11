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

#ifndef NULL_LIB
#include "tools.h"
#include "mem.h"
#include "setting.h"
#include "stat.h"

jiahome_t home[Homepages];    /* host owned page */
jiacache_t cache[Cachepages]; /* host cached page */
jiapage_t page[Maxmempages];      /* global page space */
unsigned long globaladdr; /* [0, Maxmemsize)*/

unsigned long jia_alloc3(int totalsize, int blocksize, int starthost) {
#ifdef DOSTAT
    register unsigned int begin = get_usecs();
#endif

    int homepid;
    int mapsize;
    int allocsize;
    int originaddr;
    int homei, pagei, i;
    int protect;

    jia_assert(((globaladdr + totalsize) <= Maxmemsize),
           "Insufficient shared space! --> Max=0x%x Left=0x%lx Need=0x%x\n",
           Maxmemsize, Maxmemsize - globaladdr, totalsize);

    originaddr = globaladdr;
    /* allocsize is the sum of size that will be allocated*/
    // ensure the total alloc size is multiple of pagesize (Pagesize alignment)
    allocsize = ALIGN2PAGE(totalsize);
    /* mapsize is the size that will be allocated on every host evry time */
    // ensure the block size is multiple of pagesize (Pagesize alignment)
    mapsize = ALIGN2PAGE(blocksize);
    homepid = starthost;

    while (allocsize > 0) {
        // only when current host pid == homepid, use mmap alloc space
        if (system_setting.jia_pid == homepid) {
            /* alloc page on current host(homepid) */

            jia_assert((system_setting.hosts[homepid].homesize + mapsize) < (Homepages * Pagesize),
                   "Too many home pages");
            // single: RW && multi : RO
            protect = (system_setting.hostc == 1) ? PROT_READ | PROT_WRITE : PROT_READ;
            memmap((void *)(system_setting.global_start_addr + globaladdr), (size_t)mapsize, protect);

            /**  
             * only when page on current host, set page's homei && homei's addr
             * use home array(home[homei]) to record homepage's addr, map page[pagei] with home[homei]
             */
            for (i = 0; i < mapsize; i += Pagesize) {
                pagei = (globaladdr + i) / Pagesize;
                homei = (system_setting.hosts[homepid].homesize + i) / Pagesize;
                home[homei].addr = (address_t)(system_setting.global_start_addr + globaladdr + i);
                page[pagei].homei = homei;
            }
        }

        // page array is global, homepid must be set
        for (i = 0; i < mapsize; i += Pagesize) {
            pagei = (globaladdr + i) / Pagesize;
            page[pagei].homepid = homepid;
        }

        log_info(3, "Map 0x%x bytes in home %4d! globaladdr = 0x%lx",
                        mapsize, homepid, globaladdr);

        system_setting.hosts[homepid].homesize += mapsize;
        globaladdr += mapsize;
        allocsize -= mapsize;
        homepid = (homepid + 1) % system_setting.hostc; // next host
    }

#ifdef DOSTAT
    jiastat.alloctime += get_usecs() - begin;
#endif
    return (system_setting.global_start_addr + originaddr);
}

/**
 * @brief jia_alloc4 -- alloc totalsize bytes shared memory with blocks array
 *
 * @param totalsize sum of space that allocated across all hosts (page aligned)
 * @param blocks    blocksize array, blocks[i] specify how many bytes will be allocated on every host in loop i.
 * @param n length of blocks
 * @param starthost specifies the host from which the allocation starts
 * @return start address of the allocated memory
 */
unsigned long jia_alloc4(int totalsize, int *blocks, int n, int starthost) {
    int homepid;
    int mapsize;
    int allocsize;
    int originaddr;
    int homei, pagei, i;
    int blocki;
    int protect;

#ifdef DOSTAT
    register unsigned int begin = get_usecs();
#endif

    jia_assert(((globaladdr + totalsize) <= Maxmemsize),
           "Insufficient shared space! --> Max=0x%x Left=0x%lx Need=0x%x\n",
           Maxmemsize, Maxmemsize - globaladdr, totalsize);

    blocki = 0;
    originaddr = globaladdr;
    allocsize = ALIGN2PAGE(totalsize);
    homepid = starthost;

    while (allocsize > 0) {
        mapsize = ALIGN2PAGE(blocks[blocki]);
        if (system_setting.jia_pid == homepid) {
            // alloc on host(homepid) 
            jia_assert((system_setting.hosts[homepid].homesize + mapsize) < (Homepages * Pagesize),
                   "Too many home pages");

            protect = (system_setting.hostc == 1) ? PROT_READ | PROT_WRITE : PROT_READ;
            memmap((void *)(system_setting.global_start_addr + globaladdr), (size_t)mapsize, protect);

            for (i = 0; i < mapsize; i += Pagesize) {
                pagei = (globaladdr + i) / Pagesize;
                homei = (system_setting.hosts[homepid].homesize + i) / Pagesize;
                home[homei].addr = (address_t)(system_setting.global_start_addr + globaladdr + i);
                page[pagei].homei = homei;
            }
        }

        for (i = 0; i < mapsize; i += Pagesize) {
            pagei = (globaladdr + i) / Pagesize;
            page[pagei].homepid = homepid;
        }

        log_info(3, "Map 0x%x bytes in home %4d! globaladdr = 0x%lx", mapsize,
               homepid, globaladdr);

        system_setting.hosts[homepid].homesize += mapsize;
        globaladdr += mapsize;
        allocsize -= mapsize;
        homepid = (homepid + 1) % system_setting.hostc;
        if (homepid == 0){
            blocki = (blocki + 1) % n;
        }
    }
#ifdef DOSTAT
    jiastat.alloctime += get_usecs() - begin;
#endif

    return (system_setting.global_start_addr + originaddr);
}

unsigned long jia_alloc(int totalsize) {
    /* static variable feature
     * - only be seen in the function (local static variable)
     * - only be initialized at the first call
     */
    // so, every call to the function will change starthost
    static int starthost = -1;

    starthost = (starthost + 1) % system_setting.hostc;
    return (jia_alloc3(totalsize, totalsize, starthost));
}

unsigned long jia_alloc_random(int totalsize) {
	int starthost;
	static int initialized = 0;
	if (initialized == 0) {
		srand(0);
		initialized = 1;
	}
	starthost = rand() % system_setting.hostc;
	return jia_alloc3(totalsize, totalsize, starthost);
}

unsigned long jia_alloc2(int size, int block) {
    return (jia_alloc3(size, block, 0));
}

unsigned long jia_alloc2p(int totalsize, int starthost) {
    return (jia_alloc3(totalsize, totalsize, starthost));
}

unsigned long jia_alloc_array(int totalsize, int *array, int n){
	int homepid;
	int mapsize;
	int allocsize;
	int originaddr;
    int homei, pagei;
    int protect;

#ifdef DOSTAT
    register unsigned int begin = get_usecs();
#endif


	jia_assert(((globaladdr + totalsize) <= Maxmemsize), "Insufficient shared space! --> Max=0x%x Left=0x%x, Need=0x%x\n", Maxmemsize, Maxmemsize - globaladdr, totalsize);
	jia_assert((n > 0 && n <= system_setting.hostc), "Error parameter n provided on jia_alloc_array call\n");

	homepid = 0;
	originaddr = globaladdr;
	allocsize = ALIGN2PAGE(totalsize);
	
	int i = 0; // counter
	while (allocsize > 0) {
		mapsize = ALIGN2PAGE(array[i]);
		if (system_setting.jia_pid == homepid) {
			jia_assert((system_setting.hosts[homepid].homesize + mapsize < (Homepages * Pagesize)), "Too many home pages");
		}
		protect = (system_setting.hostc == 1) ? PROT_READ | PROT_WRITE : PROT_READ;
		memmap((void *)(system_setting.global_start_addr + globaladdr), (size_t)mapsize, protect);
		
		for (i = 0; i < mapsize; i += Pagesize) {
			pagei = (globaladdr + i) / Pagesize;
			page[pagei].homepid = homepid;
		}
		
		log_info(3, "Map 0x%x bytes in home %4d! globaladdr = 0x%lx", mapsize, homepid, globaladdr);
		
		system_setting.hosts[homepid].homesize += mapsize;
		globaladdr += mapsize;
		allocsize -= mapsize;
		homepid = (homepid + 1) % system_setting.hostc;
		i = (i + 1) % n;
	}
#ifdef DOSTAT
    jiastat.alloctime += get_usecs() - begin;
#endif

    return (system_setting.global_start_addr + originaddr);
}

#else  /* NULL_LIB */

unsigned long jia_alloc(int size) {
    return (unsigned long)valloc(size);
}
#endif /* NULL_LIB */