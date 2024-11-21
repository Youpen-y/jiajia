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
 *          Author: Weiwu Hu, Weisong Shi, Zhimin Tang                 *
 * =================================================================== *
 *   This software is ported to SP2 by                                 *
 *                                                                     *
 *         M. Rasit Eskicioglu                                         *
 *         University of Alberta                                       *
 *         Dept. of Computing Science                                  *
 *         Edmonton, Alberta T6G 2H1 CANADA                            *
 * =================================================================== *
 **********************************************************************/

#ifndef JIAMEM_H
#define JIAMEM_H
#pragma once

#include "comm.h" // jia_msg_t
#include "setting.h"


#define RESERVE_TWIN_SPACE
#define REPSCHEME 0
#define Maxdiffs 64
#define SWvalve 1 /*must be less than 15*/
#define Diffunit 4
#define Dirtysize (Pagesize / (Diffunit * 8))
#define Homepages 16384 /* maximum number of home pages in a host */
#define Homesize (Homepages * Pagesize)
#define Cachesize (Pagesize * Cachepages)
#define Setnum 1 /* num of entries in a set */
#define Setpages                                                               \
    Cachepages / Setnum /* change Setpages so that have multiple sets */

#define DIFFNULL ((jia_msg_t *)NULL)
typedef unsigned char *address_t;

#define SIZ2MULSIZ(size)                                                       \
    ((size % Pagesize) == 0) ? (size) : ((size / Pagesize + 1) * Pagesize)

typedef unsigned long wtvect_t;
#define Wvbits 32
#define WVNULL 0x0
#define WVFULL 0xffffffff
#define Blocksize (Pagesize / Wvbits)

typedef struct {
    /* wtnt:
    bit0:written by home host in an interval
    bit1:written by home host in a barrier interval
    bit2:written by other host in an barrier interval
    bit7~4:single write counter
    */
    char wtnt;
    /*bit0:somebody has a valid copy*/
    char rdnt;

    address_t addr;
    wtvect_t *wtvect; /*used only for write vector*/
    address_t twin;   /*used only for write vector*/
    char wvfull;      /*used only for write vector*/
} jiahome_t;

typedef enum { UNMAP, INV, RO, RW } pagestate_t;

typedef struct {
    pagestate_t state; /*cache state: UNMAP, INV, RO, RW*/
    address_t addr;    /*cached page address */
    address_t twin;    /*cached page's twin's address */
    char wtnt;         /*write notice*/
} jiacache_t;

typedef struct {
    unsigned short int cachei;  /* cache page index */
    unsigned short int homei;   /* home page index */
    unsigned short int homepid; /* home host id */
} jiapage_t;

/* Function Declaration */

/* server */
void diffserver(jia_msg_t *);
void getpserver(jia_msg_t *req);
void diffgrantserver(jia_msg_t *);
void getpgrantserver(jia_msg_t *rep);

/* mmsync */
int replacei(int cachei);
void savepage(int cachei);
void senddiffs();
void sigsegv_handler(int signo, siginfo_t *sip, void *context);

/* mem */
void memprotect(void *addr, size_t len, int prot);
void addwtvect(int homei, wtvect_t wv, int from);
void memmap(void *addr, size_t len, int prot);
void memunmap(void *addr, size_t len);

/**
 * @brief s2l --
 *
 * @param str
 * @return unsigned long
 */
static inline unsigned long
    s2l(unsigned char *str) // TODO: unsigned long now is 8 bytes (we need to
                            // support both 32bit and 64bit machine)
{
    union {
        unsigned long l;
        // unsigned char c[Intbytes];
        unsigned char c[sizeof(unsigned char *)];
    } notype;

    notype.c[0] = str[0];
    notype.c[1] = str[1];
    notype.c[2] = str[2];
    notype.c[3] = str[3];
    notype.c[4] = str[4];
    notype.c[5] = str[5];
    notype.c[6] = str[6];
    notype.c[7] = str[7];

    return (notype.l);
}

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


extern jiapage_t page[Maxmempages]; /* global page space */


#define homehost(addr) \
    page[((unsigned long long)(addr)-system_setting.global_start_addr) / Pagesize].homepid

#define homepage(addr) \
    page[((unsigned long long)(addr)-system_setting.global_start_addr) / Pagesize].homei

#define cachepage(addr) \
    page[((unsigned long long)(addr)-system_setting.global_start_addr) / Pagesize].cachei

#endif /*JIAMEM_H*/
