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

#ifndef JIAINIT_H
#define JIAINIT_H

#define Wordsize        80
#define Linesize        200
#define Wordnum         3
#define Maxwords        10

/* maximum number of file descriptors that can be concurrently opened
        in UNIX, (>= 4*Maxhosts*Maxhosts) */
#define Maxfileno       1024 

// TODO: should recompute the cost 
#define   SEGVoverhead  600
#define   SIGIOoverhead 200
#define   ALPHAsend     151.76
#define   BETAsend      0.04
#define   ALPHArecv     327.11
#define   BETArecv      0.06 
#define   ALPHA         20
#define   BETA          20


void jia_init(int argc, char **argv);

#endif /*JIACREAT_H*/
