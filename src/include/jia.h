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
 
#ifndef JIA_PUBLIC
#define	JIA_PUBLIC
#include "setting.h"

// two meaning interface for RMA communication (unrealized)
/**
 * @brief jia_get -- get the addr of the corresponding page from its home node to local memory
 * 
 * @param addr shared memory's virtual addr
 * @return the addr of local memory(but with the new value)
 */
unsigned char *	jia_get(unsigned char *addr);

/**
 * @brief jia_put -- put the buffer's content to the addr's home node
 * 
 * @param buf the source buffer
 * @param addr the target shared memory's addr
 */
void		   jia_put(char *buf, unsigned char *addr);
// RMA end


#define jiahosts  system_setting.hostc			/* total number of hosts of a parallel program */
#define jiapid    system_setting.jia_pid		/* host identification number */

/**
 * @brief jia_init -- initialize JIAJIA system
 *
 * @param argc same as main program, argument count
 * @param argv same as main program, argument vector
 * Start copies of the application on the hosts specified in .jiahosts.
 * Also, initializes internal data structures of JIAJIA.
 */
void jia_init(int argc, char **argv);

/**
 * @brief jia_exit -- exit JIAJIA system
 */
void jia_exit();

/**
 * @brief jia_alloc3 -- allocates totalsize bytes cyclically across all hosts, each
 * time block bytes (3 parameters alloc function), start from starthost.
 *
 * @param totalsize sum of space that allocated across all hosts (page aligned)
 * @param blocksize size(page aligned) that allocated by every host every time
 * @param starthost specifies the host from which the allocation starts
 * @return start address of the allocated memory
 *
 * jia_alloc3 will allocate blocksize(page aligned) every time from starthost to
 * other hosts until the sum of allocated sizes equal totalsize(page aligned)
 */
unsigned long jia_alloc3(int totalsize,int blocksize, int starthost);

/**
 * @brief jia_alloc4 -- alloc totalsize bytes shared memory with blocks array
 *
 * @param totalsize sum of space that allocated across all hosts (page aligned)
 * @param blocks    blocksize array, blocks[i] specify how many bytes will be 
 * 					allocated on every host in loop i.
 * @param n 		length of blocks
 * @param starthost specifies the host from which the allocation starts
 * @return start address of the allocated memory
 */
unsigned long jia_alloc4(int totalsize, int *blocks, int n, int starthost);

/**
 * @brief jia_alloc2p -- alloc size(page aligned) on
 * host(proc is jiapid), two parameters alloc
 *
 * @param totalsize sum of requested shared memory
 * @param starthost host that will allocate shared memory
 * @return start address of the allocated memory
 */
unsigned long  jia_alloc2p(int totalsize, int starthost);

/**
 * @brief jia_alloc2 -- alloc totalsize shared memory on cluster with a circular allocation strategy (per blocksize)
 * 
 * @param totalsize sum of size that allocated for shared memory
 * @param blocksize size(page aligned) that allocated by every host every time
 * @return start address of the allocated memory 
 */
unsigned long  jia_alloc2(int totalsize, int blocksize);

// deprecated
// use jia_alloc2p(totalsize, 0) instead
// /**
//  * @brief jia_alloc1 -- allocate all totalsize on host 0
//  * 
//  * @param totalsize sum of size that allocated for shared memory
//  * @return start address of the allocated memory
//  */
// unsigned long  jia_alloc1(int totalsize);

/**
 * @brief jia_alloc -- allocates all totalsize bytes shared memory on starthost, but
 * the starthost depends on the times that the function have been called
 * 
 * @param totalsize sum of size that allocated for shared memory
 * @return start address of the allocated memory
 */
unsigned long  jia_alloc(int totalsize);

/**
 * @brief jia_alloc_random -- choose a host randomly, and allocate shared memory on it
 * 
 * @param totalsize sum of size that allocated for shared memory
 * @return start address of the allocated memory
 */
unsigned long jia_alloc_random(int totalsize);

/**
 * @brief jia_alloc_array -- allocate array[i] bytes shared memory on host i 
 * 
 * @param totalsize sum of size that allocated for shared memory
 * @param array blocksize array that will allocated on every host
 * @param n size of array, range: (0, jiahosts)
 * @return start address of the allocated memory
 */
unsigned long jia_alloc_array(int totalsize, int *array, int n);

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
 * @return unsigned int the start time or the elapsed time since base (us)
 */
unsigned int   jia_startstat();

/**
 * @brief jia_stopstat -- stop statistics (available with Macro DOSTAT)
 * 
 * @return unsigned int the elapsed time since base(us)
 */
unsigned int   jia_stopstat();

/**
 * @brief jia_clock() - calculate the elapsed time since program started
 *
 * @return double: time(us) elapsed since program started
 */
double          jia_clock();

/**
 * @brief jia_send -- an MPI-similar call, send len bytes of buf to host topid
 * 
 * @param buf pointer to buffer
 * @param len length of bytes
 * @param topid target host
 * @param tag scope
 */
void           jia_send(char* buf, int len, int topid, int tag);

/**
 * @brief jia_recv -- an MPI-similar call, receive len bytes from host frompid to buf
 * 
 * @param buf pointer to buffer
 * @param len length of bytes
 * @param frompid source host
 * @param tag scope
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
