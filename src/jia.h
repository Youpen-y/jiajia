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
 
#ifndef JIA_PUBLIC
#define	JIA_PUBLIC

#define jiahosts  hostc
#define jiapid    jia_pid

extern int		jia_pid;
extern int      hostc;

/**
 * @brief jia_init -- initialize JIAJIA
 * Start copies of the application on the hosts specified in .jiahosts.
 * Also, initializes internal data structures of JIAJIA.
 */
void           jia_init(int, char **);

/**
 * @brief jia_exit -- shut JIAJIA down
 */
void           jia_exit();

/**
 * @brief jia_alloc3 -- allocated shared memory.
 * @param size: indicates the number of bytes allocated
 * @param others: allow the programmer to control data distribution across hosts to improve performance
 * @return unsigned long 
 */
unsigned long  jia_alloc3(int size,int,int);
unsigned long  jia_alloc2(int,int);
unsigned long  jia_alloc2p(int, int);
unsigned long  jia_alloc1(int);
unsigned long  jia_alloc(int);

/**
 * @brief jia_lock -- acquire a lock specified by lockid
 * 
 * @param lockid id of lock
 */
void           jia_lock(int lockid);

/**
 * @brief jia_unlock -- release a lock specified by lockid
 * 
 * @param lockid id of lock
 */
void           jia_unlock(int lockid);

/**
 * @brief jia_barrier -- perform a global barrier
 * 
 * @note jia_barrier() cannot be called inside a critical section enclosed by jia_lock() and jia_unlock()
 */
void           jia_barrier();

/**
 * @brief jia_wait -- simple synchronization mechanism that requires
 *  all processors to wait altogether before going ahead.
 * 
 * @note does not enforce any coherence operations across processors
 */
void           jia_wait();

/**
 * @brief jia_setcv -- set conditional variable cv
 * 
 * @param cv conditional variable
 */
void           jia_setcv(int cv);

/**
 * @brief jia_resetcv -- reset conditional variable cv
 * 
 * @param cv conditional variable
 */
void           jia_resetcv(int cv);

/**
 * @brief jia_waitcv -- wait on conditional variable cv
 * 
 * @param cv conditional variable
 */
void           jia_waitcv(int cv);

/**
 * @brief jia_error -- print out the err string and shut down all processes started by jia_init()
 * 
 * @param ... 
 */
void           jia_error(char*, ...);

/**
 * @brief jia_startstat -- start statistics (available with Macro DOSTAT)
 * 
 * @return unsigned int 
 */
unsigned int   jia_startstat();

/**
 * @brief jia_stopstat -- stop statistics (available with Macro DOSTAT)
 * 
 * @return unsigned int 
 */
unsigned int   jia_stopstat();

/**
 * @brief jia_clock -- elapsed time since start of application in seconds
 * 
 * @return float 
 */
float          jia_clock();

/**
 * @brief jia_send -- an MPI-similar call, send len bytes of buf to host topid
 * 
 */
void           jia_send(char* buf, int len, int topid, int tag);

/**
 * @brief jia_recv -- an MPI-similar call, receive len bytes from host frompid to buf
 * 
 * @return int 
 */
int            jia_recv(char* buf, int len, int frompid, int tag);

/**
 * @brief jia_reduce -- an MPI-like call, reduce cnt numbers from all hosts to host root with operation op
 * 
 */
void           jia_reduce(char* snd, char* rcv, int cnt, int op, int root);

/**
 * @brief jia_bcast -- an MPI-similar call, send len bytes from buf of host root to buf of all hosts
 * 
 */
void           jia_bcast(char* buf, int len, int root);

/**
 * @brief jia_config -- System configuration
 * 
 * @note turn on/off optimization methods, such as home migration, write vector, adaptive write detection
 */
void           jia_config(int, int);

/**
 * @brief jia_divtask -- divide task across processors
 * 
 */
void           jia_divtask(int* begin, int* end);

/**
 * @brief jia_loadcheck -- check loads of all processors
 * 
 */
void           jia_loadcheck();

#ifndef ON
#define OFF    0
#define ON     1
#endif

#define HMIG       0
#define ADWD       1
#define BROADCAST  2
#define LOADBAL    3 
#define WVEC       4 

#define jia_wtnt(a,b) 
#define jia_wtntw(a) 
#define jia_wtntd(a) 
#define jia_errexit jia_error

/* argonne (ANL) macros */

extern int      jia_lock_index;

#define Maxlocks   64
#define	LOCKDEC(x)	int	x;
#ifndef NULL_LIB
#define	LOCKINIT(x)     (x)=(jia_lock_index++)%Maxlocks;
#else /* NULL_LIB */
#define	LOCKINIT(x)     
#endif /* NULL_LIB */
#define	LOCK(x)		jia_lock(x);
#define	UNLOCK(x)	jia_unlock(x);

#define	ALOCKDEC(x,y)	int x[y];
#define	ALOCKINIT(x,y)	{						\
			int	i;	 			\
			for (i=0;i<(y);i++){ 			\
				(x)[i] = (jia_lock_index++)%Maxlocks;	\
			}					\
			}
#define	ALOCK(x,y)	jia_lock((x)[y]);
#define	AULOCK(x,y)	jia_unlock((x)[y]);

#define	BARDEC(x)	int	x;
#define	BARINIT(x)
#define	BARRIER(x,y)	jia_barrier();

#define	EXTERN_ENV		
#define	MAIN_ENV		
#define	MAIN_INITENV(x,y)			
#define	MAIN_END 
#define	WAIT_FOR_END(x)	jia_barrier();

#define	PAUSEDEC(x)	int	x;
#define	PAUSEINIT(x)	
#define	PAUSE_POLL(x)
#define	PAUSE_SET(x,y)	
#define	PAUSE_RESET(x)	
#define	PAUSE_ACCEPT(x)	

#define	GSDEC(x)	int	x;
#define	GSINIT(x)	
#define	GETSUB(x, y, z, zz)	
/*
#define	G_MALLOC(x)	jia_alloc3((x),(x)/jiahosts,0);
*/
#define	G_MALLOC(x)	jia_alloc1((x));
#define	G2_MALLOC(x,y)	jia_alloc3(x);

#define	CLOCK(x)	x=jia_clock();

#endif /* JIA_PUBLIC */
