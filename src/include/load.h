/********************************************************
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

#ifndef JIALOAD_H
#define JIALOAD_H

#include "global.h"
#include "comm.h"
#define  Lperiod   120
#define  Delta     0.05

typedef struct loadtype {
        float   power;
        float   time;
        int     begin;
        int     end; 
} jiaload_t;

extern jiaload_t loadstat[Maxhosts];

/* function declaration */

/**
 * @brief jia_loadbalance -- send LOADREQ msg to master
 * 
 */
void jia_loadbalance();

/**
 * @brief jia_newload --
 * 
 */
void jia_newload();

/**
 * @brief loadserver -- 
 * 
 * @param msg 
 */
void loadserver(jia_msg_t *msg);

/**
 * @brief loadgrantserver -- get every host's power from loadgrant msg
 * 
 * @param msg 
 */
void loadgrantserver(jia_msg_t *msg);

/**
 * @brief jia_loadcheck() - check and record computation power of each processor
 * in the system
 *
 * @note Total computation power of all processors are always normalized to 1
 *
 * Processor computation power = Old computation power / computation time since
 * last jia_loadcheck() call
 */
void jia_loadcheck();

/**
 * @brief jia_divtask -- 
 * 
 * @param begin 
 * @param end 
 */
void jia_divtask(int *begin, int *end);

#endif /*JIAMEM_H*/
