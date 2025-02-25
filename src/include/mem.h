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
 **********************************************************************/

#ifndef JIAMEM_H
#define JIAMEM_H
#pragma once

#include "comm.h" // jia_msg_t
#include "setting.h"
#include <signal.h>


#define RESERVE_TWIN_SPACE
#define REPSCHEME 0
#define Maxdiffs 64
#define SWvalve 1 /*must be less than 15*/
#define Diffunit 4
#define Dirtysize (Pagesize / (Diffunit * 8))
#define Homepages 16384 /* maximum number of home pages in a host */
#define Homesize (Homepages * Pagesize)
#define Cachesize (Pagesize * Cachepages)
#define Setnum 1 /* num of set */
#define Setpages                                                               \
    Cachepages / Setnum /* page num of a set */

#define DIFFNULL ((slot_t *)NULL)
typedef unsigned char *address_t;

#define ALIGN2PAGE(size)                                                       \
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
typedef enum {RANDOM, CIRCULAR, LRU} repscheme_t;   // cache replacement scheme

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

extern jiapage_t page[Maxmempages]; /* global page space */
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

/* cache */
int findposition(address_t addr);

#define homehost(addr) \
    page[((unsigned long long)(addr)-system_setting.global_start_addr) / Pagesize].homepid

#define homepage(addr) \
    page[((unsigned long long)(addr)-system_setting.global_start_addr) / Pagesize].homei

#define cachepage(addr) \
    page[((unsigned long long)(addr)-system_setting.global_start_addr) / Pagesize].cachei

#endif /*JIAMEM_H*/
