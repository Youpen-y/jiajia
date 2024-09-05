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
#include "global.h"
#include "mem.h"
#include "init.h"
#include "comm.h"
#include "syn.h"

#ifdef IRIX62
#include <sys/sbd.h>
#endif /* IRIX62 */

extern jia_msg_t *newmsg();
extern void freemsg(jia_msg_t*);
extern void asendmsg(jia_msg_t *req);
extern void savepage(int cachei);
extern void newtwin(address_t *twin);
extern void freetwin(address_t *twin);
extern void enable_sigio();
extern void disable_sigio();
extern void jia_barrier();
extern float jia_clock();

extern void assert0(int, char *);
extern unsigned int get_usecs();
extern void appendmsg(jia_msg_t *, unsigned char *, int);
extern void assert(int cond, char *amsg);

void initmem();
void getpage(address_t addr,int flag);
int xor(address_t addr);
void flushpage(int cachei);
int replacei(int cachei);
int findposition(address_t addr);
#ifdef SOLARIS
void sigsegv_handler(int signo, siginfo_t *sip, ucontext_t *uap);
#endif /* SOLARIS */

#if defined AIX41 || defined IRIX62
void sigsegv_handler();
#endif /* AIX41 || IRIX62 */

#ifdef LINUX 
// void sigsegv_handler(int, struct sigcontext);
void sigsegv_handler(int, siginfo_t *, void *);
#endif

void getpserver(jia_msg_t *req);
void getpgrantserver(jia_msg_t *req);
void addwtvect(int homei,wtvect_t wv,int from);
void setwtvect(int homei,wtvect_t wv);
unsigned long s2l(unsigned char *str);
void diffserver(jia_msg_t *req);
void diffgrantserver(jia_msg_t *req);
int  encodediff(int diffi, unsigned char* diff);
void senddiffs();
void savediff(int cachei);

extern int         jia_pid;
extern host_t      hosts[Maxhosts];
extern int         hostc;
extern char        errstr[Linesize];
extern jiastack_t  lockstack[Maxstacksize];
extern jialock_t   locks[Maxlocks+1];
extern int         stackptr;
extern int         H_MIG,B_CAST,W_VEC;

#ifdef DOSTAT
extern jiastat_t jiastat;
extern int statflag;
#endif

jiahome_t       home[Homepages+1];    /* host owned page */
jiacache_t      cache[Cachepages+1];  /* host cached page */
jiapage_t       page[Maxmempages];    /* global page space */

unsigned long   globaladdr; /* [0, Maxmemsize)*/
long            jiamapfd;   /* file descriptor of the file that mapped to process's virtual address space */
volatile int getpwait;
volatile int diffwait;
int repcnt[Setnum]; /* record the last replacement index of every set */
jia_msg_t *diffmsg[Maxhosts];   /* store every host's diff msgs */


/**
 * @brief initmem - initialize memory setting (wait for SIGSEGV signal)
 * 
 * step1: initialize diff msg for every host and some condition variables(diffwait, getpwait)
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
void initmem()
{
  int i, j;

  for (i = 0; i < Maxhosts; i++) { // step1
    diffmsg[i] = DIFFNULL;
  }
  diffwait = 0;
  getpwait = 0;

  for (i = 0; i <= hostc; i++) {  // step2: set every host's homesize to 0
    hosts[i].homesize=0;
  }

  for (i = 0; i <= Homepages; i++) {
    home[i].wtnt=0;
    home[i].rdnt=0;
    home[i].addr=(address_t)0;
    home[i].twin=NULL;
  }

  for (i = 0; i < Maxmempages; i++) {
    page[i].cachei=(unsigned short)Cachepages;
    page[i].homei=(unsigned short)Homepages;
    page[i].homepid=(unsigned short)Maxhosts;
  }

  for (i = 0; i <= Cachepages; i++) {
    cache[i].state=UNMAP;   /* initial cached page state is UNMAP */
    cache[i].addr=0;
    cache[i].twin=NULL;
    cache[i].wtnt=0;
  }

  globaladdr=0;

  #if defined SOLARIS || defined IRIX62
  jiamapfd=open("/dev/zero", O_RDWR,0);

  { struct sigaction act;

    //act.sa_handler = (void_func_handler)sigsegv_handler;
    act.sa_sigaction = (void_func_handler)sigsegv_handler;
    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_SIGINFO;
    if (sigaction(SIGSEGV, &act, NULL))
      assert0(0,"segv sigaction problem");
  }
  #endif 

  #ifdef LINUX
  jiamapfd=open("/dev/zero", O_RDWR, 0); /* file descriptor refer to the open file */
  { /* reference to a non-home page causes the delivery of a SIGSEGV signal,
    the SIGSEGV handler then maps the fault page to the global address of the page in local address space */
    struct sigaction act;
    act.sa_handler = (void_func_handler)sigsegv_handler;
    sigemptyset(&act.sa_mask);
    // act.sa_flags = SA_NOMASK; 
    // act.sa_flags = SA_NODEFER;  /* SA_NOMASK is obsolete */
    act.sa_flags = SA_NODEFER | SA_SIGINFO;
    if (sigaction(SIGSEGV, &act, NULL))
      assert0(0,"segv sigaction problem");
  }
  #endif 

  #ifdef AIX41
    { struct sigvec vec;

      vec.sv_handler = (void_func_handler)sigsegv_handler;
      vec.sv_flags = SV_INTERRUPT;
      sigvec(SIGSEGV, &vec, NULL);
    }
  #endif /* SOLARIS */
  
  for (i=0; i<Setnum; i++) { // TODO: consider multiple sets
    repcnt[i]=0;
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
void memprotect(void* addr, size_t len, int prot)                               
{
  int protyes;                                            

  protyes = mprotect(addr,len,prot);  
  if (protyes != 0) {                                        
    sprintf(errstr,"mprotect failed! addr=0x%lx, errno=%d",(unsigned long)addr, errno); 
    assert(0, errstr);                          
  }                                      
}

/**
 * @brief memmap - map jiamapfd file to process's address space [addr, addr+len-1]
 * 
 * @param addr start address of the mapped memory
 * @param len length of mapped memory
 * @param prot mapped memory's protection level (PROT_READ/PROT_WRITE/PROT_EXEC/PROT_NONE)
 */
void memmap(void* addr, size_t len, int prot)                               
{
  void* mapad;                                                  

#if defined SOLARIS || defined IRIX62 || defined LINUX
  // map file descriptor jiamapfd refered file to process virtual memory [addr, addr+length-1] with protection level prot
  mapad=mmap(addr, len, prot, MAP_PRIVATE|MAP_FIXED, jiamapfd, 0);
#endif 
#ifdef AIX41
  mapad=mmap(addr, len, prot, MAP_PRIVATE|MAP_FIXED|MAP_ANONYMOUS, -1, 0);
#endif 

  if (mapad != addr){                                      
    sprintf(errstr,"mmap failed! addr=0x%lx, errno=%d",(unsigned long)(addr),errno);
    assert(0,errstr);                                                
  }                                                                  
}


/**
 * @brief memunmap - unmap memory, remove the mapped memory from process virtual address space 
 * 
 * @param addr start address of virtual mapped memory that should be cancelled ()
 * @param len length of mapped memory that need to removed
 */
void memunmap(void* addr,size_t len)                               
{
  int unmapyes;                                            

  unmapyes=munmap(addr,len);  
  if (unmapyes != 0) {                                   
    sprintf(errstr,"munmap failed! addr=0x%lx, errno=%d",(unsigned long)addr,errno); 
    assert(0,errstr);                   
  }                                                       
}

/**
 * @brief jia_alloc3 -- allocates size bytes cyclically across all hosts, each time block bytes
 * 
 * @param size the sum of space that allocated across all hosts (page aligned)
 * @param block the size(page aligned) that allocated by every host every time
 * @param starthost specifies the host from which the allocation starts
 * @return unsigned long 
 * 
 * jia_alloc3 will allocate block(page aligned) every time from starthost to other hosts until the sum equal size(page aligned)
 */
unsigned long jia_alloc3(int size, int block, int starthost)
{
  int homepid;
  int mapsize;
  int allocsize;
  int originaddr;
  int homei, pagei, i;
  int protect;
  
  sprintf(errstr, 
          "Insufficient shared space! --> Max=0x%x Left=0x%lx Need=0x%x\n",
          Maxmemsize, Maxmemsize-globaladdr,size);
  assert(((globaladdr+size)<=Maxmemsize), errstr);

  originaddr = globaladdr;
  /* allocsize is the sum of size that will be allocated*/
  allocsize = ((size%Pagesize)==0)?(size):((size/Pagesize+1)*Pagesize);   // ensure the alloc size is multiple of pagesize 
  /* mapsize is the size that will be allocated on every host evry time */
  mapsize = ((block%Pagesize)==0)?(block):((block/Pagesize+1)*Pagesize);  // ensure the block size is multiple of pagesize
  homepid = starthost;

  while (allocsize > 0) {
    if (jia_pid == homepid) { // only when current host pid == homepid, use mmap alloc space
      assert((hosts[homepid].homesize+mapsize)<(Homepages*Pagesize),"Too many home pages");

      protect = (hostc==1) ? PROT_READ|PROT_WRITE : PROT_READ;
      memmap((void *)(Startaddr+globaladdr), (size_t)mapsize, protect);

      for (i=0; i<mapsize; i+=Pagesize) {
        pagei=(globaladdr+i)/Pagesize;
        homei=(hosts[homepid].homesize+i)/Pagesize;
        home[homei].addr=(address_t)(Startaddr+globaladdr+i);
        page[pagei].homei=homei;
      }
    }

    for (i = 0; i < mapsize; i += Pagesize) { // set page's homepid
      pagei=(globaladdr+i)/Pagesize;
      page[pagei].homepid=homepid;
    }

    if (jia_pid == homepid) {
      printf("Map 0x%x bytes in home %4d! globaladdr = 0x%lx\n", mapsize, homepid, globaladdr);
    }

    hosts[homepid].homesize+=mapsize;
    globaladdr+=mapsize;
    allocsize-=mapsize;   
    homepid=(homepid+1)%hostc;  // next host
  }
  return (Startaddr+originaddr);
}

/**
 * @brief jia_alloc3b -- 
 * 
 * @param size 
 * @param block 
 * @param starthost 
 * @return unsigned long 
 */
unsigned long jia_alloc3b(int size,int *block, int starthost)
{
  int homepid;
  int mapsize;
  int allocsize;
  int originaddr;
  int homei,pagei,i;
  int blocki;
  int protect;

  sprintf(errstr, 
          "Insufficient shared space! --> Max=0x%x Left=0x%lx Need=0x%x\n",
          Maxmemsize, Maxmemsize-globaladdr,size);
  
  assert(((globaladdr+size)<=Maxmemsize), errstr);

  blocki=0; 
  originaddr=globaladdr;
  allocsize=((size%Pagesize)==0)?(size):((size/Pagesize+1)*Pagesize);
  homepid=starthost;
  
  while (allocsize>0){
    mapsize=((block[blocki]%Pagesize)==0)?(block[blocki]):((block[blocki]/Pagesize+1)*Pagesize);
    if (jia_pid==homepid){
      assert((hosts[homepid].homesize+mapsize)<(Homepages*Pagesize),"Too many home pages");

      protect=(hostc==1) ? PROT_READ|PROT_WRITE : PROT_READ;
      memmap((void *)(Startaddr+globaladdr),(size_t)mapsize,protect);

      for (i=0;i<mapsize;i+=Pagesize){
        pagei=(globaladdr+i)/Pagesize;
        homei=(hosts[homepid].homesize+i)/Pagesize;
        home[homei].addr=(address_t)(Startaddr+globaladdr+i);
        page[pagei].homei=homei;
      }
    }
      
    for (i=0;i<mapsize;i+=Pagesize){
      pagei=(globaladdr+i)/Pagesize;
      page[pagei].homepid=homepid;
    }
  
  #ifdef JIA_DEBUG
  #endif 
    printf("Map 0x%x bytes in home %4d! globaladdr = 0x%lx\n",mapsize,homepid,globaladdr);
        
    hosts[homepid].homesize+=mapsize;
    globaladdr+=mapsize;
    allocsize-=mapsize;
    homepid=(homepid+1)%hostc;
    if (homepid==0) blocki++;
  }
  
  return(Startaddr+originaddr);
}


unsigned long jia_alloc(int size)
{
  static int starthost=-1;

  starthost=(starthost+1)%hostc;
  return(jia_alloc3(size,size,starthost));
}

unsigned long jia_alloc1(int size)
{
  return(jia_alloc3(size,size,0));
}

unsigned long jia_alloc2(int size, int block)
{
  return(jia_alloc3(size,block,0));
}

/**
 * @brief jia_alloc2p -- two parameters alloc, alloc size(page aligned) on host(proc is jiapid) 
 * 
 * @param size memory size that will be allocated(page aligned)
 * @param proc host(jia_pid == proc)
 * @return unsigned long 
 */
unsigned long jia_alloc2p(int size, int proc)
{
  return(jia_alloc3(size,size,proc));
}

/**
 * @brief xor -- get the index of the first page of the set based on the addr
 * 
 * @param addr address of a bytes
 * @return int 
 */
int xor(address_t addr)
{
  return((((unsigned long)(addr-Startaddr)/Pagesize)%Setnum)*Setpages);
}

/**
 * @brief replacei - 
 * 
 * @param cachei: index of cache
 * @return int 
 */
int replacei(int cachei)  // TODO: implement LRU replacement 
{
  int seti;

  if (REPSCHEME==0) // replace scheme equals to zero, random replacement
    return((random()>>8)%Setpages);
  else{ // circular replacement in corresponding set
    seti=cachei/Setpages;
    repcnt[seti]=(repcnt[seti]+1)%Setpages;
    return(repcnt[seti]);
  } 
}

/**
 * @brief flushpage -- 
 * 
 * @param cachei page index in cache
 */
void flushpage(int cachei)
{
  memunmap((void*)cache[cachei].addr,Pagesize);

  page[((unsigned long)cache[cachei].addr-Startaddr)/Pagesize].cachei=Cachepages;

  if (cache[cachei].state==RW) freetwin(&(cache[cachei].twin));
  cache[cachei].state=UNMAP;
  cache[cachei].wtnt=0;
  cache[cachei].addr=0;
}

/**
 * @brief findposition -- 
 * 
 * @param addr 
 * @return int 
 */
int findposition(address_t addr)
{
  int cachei;             /*index of cache*/
  int seti;               /*index in a set*/
  int invi;
  int i;
  
  cachei=xor(addr);
  seti=replacei(cachei);
  invi=-1;
  for (i=0;(cache[cachei+seti].state!=UNMAP)&&(i<Setpages);i++){

    if ((invi==(-1))&&(cache[cachei+seti].state==INV))
      invi=seti;

    seti=(seti+1)%Setpages;
  }

  if ((cache[cachei+seti].state!=UNMAP)&&(invi!=(-1))){
    seti=invi;
  }   

  if ((cache[cachei+seti].state==INV)||(cache[cachei+seti].state==RO)){
    flushpage(cachei+seti);
#ifdef DOSTAT
if (statflag==1){
  if (cache[cachei+seti].state==RO) jiastat.repROcnt++;
}
#endif
  }else if (cache[cachei+seti].state==RW){
    savepage(cachei+seti);
    senddiffs();
    while(diffwait);
    flushpage(cachei+seti);
#ifdef DOSTAT
if (statflag==1){
  jiastat.repRWcnt++;
}
#endif
  }
  page[((unsigned long)addr-Startaddr)/Pagesize].cachei=(unsigned short)(cachei+seti);
  return(cachei+seti);
}


#ifdef SOLARIS 
void sigsegv_handler(int signo, siginfo_t *sip, ucontext_t *uap)
#endif 

#if defined AIX41 || defined IRIX62
void sigsegv_handler(int signo, int code, struct sigcontext *scp, char *addr) 
#endif 

#ifdef LINUX
//void sigsegv_handler(int signo, struct sigcontext sigctx)
void sigsegv_handler(int signo, siginfo_t *sip, void *context)
#endif 
{
  address_t faultaddr;
  int writefault;
  int cachei, homei;

  sigset_t set;
    
  #ifdef DOSTAT
  register unsigned int begin = get_usecs();
  if (statflag==1){
  jiastat.kernelflag=2;
  }
  #endif

  sigemptyset(&set);
  sigaddset(&set,SIGIO);
  sigprocmask(SIG_UNBLOCK, &set, NULL);

  #ifdef SOLARIS
  faultaddr=(address_t)sip->si_addr;
  faultaddr=(address_t)((unsigned long)faultaddr/Pagesize*Pagesize);
  writefault=(int)(*(unsigned *)uap->uc_mcontext.gregs[REG_PC] & (1<<21));
  #endif 

  #ifdef AIX41 
  faultaddr = (char *)scp->sc_jmpbuf.jmp_context.o_vaddr;
  faultaddr = (address_t)((unsigned long)faultaddr/Pagesize*Pagesize);
  writefault = (scp->sc_jmpbuf.jmp_context.except[1] & DSISR_ST) >> 25;
  #endif 

  #ifdef IRIX62
  faultaddr=(address_t)scp->sc_badvaddr;
  faultaddr=(address_t)((unsigned long)faultaddr/Pagesize*Pagesize);
  writefault=(int)(scp->sc_cause & EXC_CODE(1));
  #endif 

  #ifdef LINUX 
  // faultaddr = (address_t) sigctx.cr2;
  // faultaddr = (address_t) ((unsigned long)faultaddr/Pagesize*Pagesize); // page aligned
  // writefault = (int) sigctx.err & 2;
  faultaddr = (address_t) sip->si_addr;
  faultaddr = (address_t) ((unsigned long)faultaddr/Pagesize*Pagesize);
  writefault = sip->si_code & 2;  /* si_code: 1 means that address not mapped to object => ()
                                     si_code: 2 means that invalid permissions for mapped object => ()*/
  #endif 
  printf("Enter sigsegv handler\n");
  printf("Shared memory out of range from %p to %p!, faultaddr=%p, writefault=%d\n",
          Startaddr, Startaddr+globaladdr, faultaddr, writefault);

  printf("sig info structure siginfo_t\n");
  printf("\tsignal err : %d \n"
         "\tsignal code: %d \n"
         "\t    si_addr: %p\n", sip->si_errno, sip->si_code, sip->si_addr);

  sprintf(errstr,"Access shared memory out of range from 0x%x to 0x%x!, faultaddr=0x%x, writefault=0x%x", 
                  Startaddr, Startaddr+globaladdr, faultaddr, writefault);
  assert((((unsigned long)faultaddr<(Startaddr+globaladdr))&& 
         ((unsigned long)faultaddr>=Startaddr)), errstr);


  if (homehost(faultaddr)==jia_pid){  // page's home is current host (si_code = 2)
    memprotect((caddr_t)faultaddr,Pagesize,PROT_READ|PROT_WRITE); // grant write right
    homei=homepage(faultaddr);
    home[homei].wtnt|=3;  /* set bit0 = 1, bit1 = 1 */
    if ((W_VEC==ON)&&(home[homei].wvfull==0)){
      newtwin(&(home[homei].twin));
      memcpy(home[homei].twin,home[homei].addr,Pagesize);
    }
#ifdef DOSTAT
if (statflag==1){
jiastat.segvLtime += get_usecs() - begin;
jiastat.kernelflag=0;
jiastat.segvLcnt++;
}
#endif
  }else{ // page's home is not current host (si_code = 1, )
    writefault=(writefault==0) ? 0 : 1;
    cachei=(int)page[((unsigned long)faultaddr-Startaddr)/Pagesize].cachei;
    if (cachei<Cachepages){
      memprotect((caddr_t)faultaddr,Pagesize,PROT_READ|PROT_WRITE);
      if (!((writefault)&&(cache[cachei].state==RO))){
        getpage(faultaddr,1);
      }
    }else{
      cachei=findposition(faultaddr);
      memmap((caddr_t)faultaddr,Pagesize,PROT_READ|PROT_WRITE);
      getpage(faultaddr,0);
    }

    if (writefault){
      cache[cachei].addr=faultaddr;
      cache[cachei].state=RW;
      cache[cachei].wtnt=1;
      newtwin(&(cache[cachei].twin));
      while(getpwait) ;
      memcpy(cache[cachei].twin,faultaddr,Pagesize);
    }else{
      cache[cachei].addr=faultaddr;
      cache[cachei].state=RO;
      while(getpwait) ;
      memprotect((caddr_t)faultaddr,(size_t)Pagesize,PROT_READ);
    }

#ifdef DOSTAT
if (statflag==1){
jiastat.segvRcnt++;
jiastat.segvRtime += get_usecs() - begin;
jiastat.kernelflag=0;
}
#endif
  }
  printf("Out sigsegv_handler\n\n");
}

/**
 * @brief getpage -- according to addr, get page from remote host (page's home)
 * 
 * @param addr page global address
 * @param flag indicate read(0) or write(1) request
 */
void getpage(address_t addr,int flag)
{
  int homeid;
  jia_msg_t *req;

  homeid=homehost(addr);
  assert((homeid!=jia_pid),"This should not have happened 2!");
  req=newmsg();

  req->op=GETP;
  req->frompid=jia_pid;
  req->topid=homeid;
  req->temp=flag;       /*0:read request, 1:write request*/
  req->size=0;
  //appendmsg(req,ltos(addr),Intbytes);
  appendmsg(req, ltos(addr), sizeof(unsigned char *));
  getpwait=1;
  asendmsg(req);

  freemsg(req);
  while(getpwait) ;
#ifdef DOSTAT
if (statflag==1){
  jiastat.getpcnt++;
}
#endif
}


/**
 * @brief getpserver -- 
 * 
 * @param req 
 */
void getpserver(jia_msg_t *req)
{
  address_t paddr; 
  int homei;
  jia_msg_t *rep;

  assert((req->op==GETP)&&(req->topid==jia_pid),"Incorrect GETP Message!");

  paddr=(address_t)stol(req->data); // getp message data is the page's addr
/*
 printf("getpage=0x%x from host %d\n",(unsigned long) paddr,req->frompid);
*/
  if ((H_MIG==ON)&&(homehost(paddr)!=jia_pid)){
    /*This is a new home, the home[] data structure may
      not be updated, but the page has already been here
      the rdnt item of new home is set to 1 in migpage()*/
  }else{
    assert((homehost(paddr)==jia_pid),"This should have not been happened!");
    homei=homepage(paddr);

    if ((W_VEC==ON)&&(home[homei].wvfull==1)){
      home[homei].wvfull=0;
      newtwin(&(home[homei].twin));
      memcpy(home[homei].twin,home[homei].addr,Pagesize);
    }

    home[homei].rdnt=1;
  }
  rep=newmsg();
  rep->op=GETPGRANT;
  rep->frompid=jia_pid;
  rep->topid=req->frompid;
  rep->temp=0;
  rep->size=0;
  //appendmsg(rep,req->data,Intbytes);  // reply msg data format [req->data(4bytes), pagedata(4096bytes)], req->data is the page start address
  appendmsg(rep, req->data, sizeof(unsigned char *));

  if ((W_VEC==ON)&&(req->temp==1)){int i;
    for (i=0;i<Wvbits;i++){
      if (((home[homei].wtvect[req->frompid]) & (((wtvect_t)1)<<i))!=0){
        appendmsg(rep,paddr+i*Blocksize,Blocksize);
      }
    }
    rep->temp=home[homei].wtvect[req->frompid];
  }else{
    printf("possible bug point\n");
    appendmsg(rep,paddr,Pagesize);
    rep->temp=WVFULL;
  }

  if (W_VEC==ON){
    home[homei].wtvect[req->frompid]=WVNULL;
  /*
    printf("0x%x\n",rep->temp);
  */
  }

  asendmsg(rep);
  freemsg(rep);
}

/**
 * @brief getpgrantserver -- getpgrant server
 * 
 * @param rep 
 */
void getpgrantserver(jia_msg_t *rep)
{
  address_t addr;
  unsigned int datai;
  unsigned long wv;
  int i;
    
  assert((rep->op==GETPGRANT),"Incorrect returned message!");
  
  wv=rep->temp;

  datai=0;
  addr=(address_t)stol(rep->data+datai);
  datai+=Intbytes;

  if ((W_VEC==ON)&&(wv!=WVFULL)){
    for (i=0;i<Wvbits;i++){
      if ((wv & (((wtvect_t)1)<<i))!=0){
        memcpy(addr+i*Blocksize,rep->data+datai,Blocksize);
        datai+=Blocksize;
      }
    }
  }else{
    printf("addr is %#x , rep->data+datai = %#x\n", addr, rep->data+datai);
    memcpy((unsigned char *)addr,rep->data+datai,Pagesize);  // TODO:possible bug
    printf("I have copy the page from remote home to %#x\n", addr);
  }

  getpwait=0;
}


void addwtvect(int homei,wtvect_t wv,int from)
{
  int i;

  home[homei].wvfull=1;
  for (i=0;i<hostc;i++){
    if (i!=from) home[homei].wtvect[i] |= wv;

    if (home[homei].wtvect[i]!=WVFULL) home[homei].wvfull=0;
  }
}


void setwtvect(int homei,wtvect_t wv)
{
  int i;

  home[homei].wvfull=1;
  for (i=0;i<hostc;i++){
    home[homei].wtvect[i] = wv;
    if (home[homei].wtvect[i]!=WVFULL) home[homei].wvfull=0;
  }
}


unsigned long s2l(unsigned char *str)
{
  union {
    unsigned long l;
    unsigned char c[Intbytes];
       } notype;
 
  notype.c[0]=str[0];
  notype.c[1]=str[1];
  notype.c[2]=str[2];
  notype.c[3]=str[3];

  return(notype.l);
}


void diffserver(jia_msg_t *req)
{
  int datai;
  int homei;
  unsigned long pstop,doffset,dsize;
  unsigned long paddr;
  jia_msg_t *rep;
  wtvect_t wv;

#ifdef DOSTAT
  register unsigned int begin = get_usecs();
#endif

  assert((req->op==DIFF)&&(req->topid==jia_pid),"Incorrect DIFF Message!");

  datai=0;
  while(datai<req->size){
   paddr=s2l(req->data+datai);
   datai+=Intbytes;
   wv=WVNULL;

   homei=homepage(paddr);
   /* In the case of H_MIG==ON, homei may be the index of 
      the new home[] which has not be updated due to difference
      of barrier arrival. In this case, homei==Homepages and
      home[Homepages].wtnt==0*/

   if ((home[homei].wtnt&1)!=1)
     memprotect((caddr_t)paddr,Pagesize,PROT_READ|PROT_WRITE);

   pstop=s2l(req->data+datai)+datai-Intbytes;
   datai+=Intbytes;
   while(datai<pstop){
     dsize=s2l(req->data+datai) & 0xffff;
     doffset=(s2l(req->data+datai)>>16) & 0xffff;
     datai+=Intbytes;
     memcpy((address_t)(paddr+doffset),req->data+datai,dsize);
     datai+=dsize;

     if ((W_VEC==ON)&&(dsize>0)) {int i;
       for(i=doffset/Blocksize*Blocksize;i<(doffset+dsize);i+=Blocksize)
         wv |= ((wtvect_t)1)<<(i/Blocksize);
     }
   }

   if (W_VEC==ON) addwtvect(homei,wv,req->frompid);

   if ((home[homei].wtnt&1)!=1)
     memprotect((caddr_t)paddr,(size_t)Pagesize,(int)PROT_READ);

#ifdef DOSTAT
   if (statflag==1){
     jiastat.mwdiffcnt++;
   }
#endif
 }

#ifdef DOSTAT
if (statflag==1){
 jiastat.dedifftime += get_usecs() - begin;
 jiastat.diffcnt++;
}
#endif

 rep=newmsg();
 rep->op=DIFFGRANT;
 rep->frompid=jia_pid;
 rep->topid=req->frompid;
 rep->size=0;

 asendmsg(rep);
 freemsg(rep);
}

/**
 * @brief encodediff() -- encode the diff of cache page(cachei) and its twin to the paramater diff
 * 
 * @param cachei cache page index
 * @param diff address that used to save difference 
 * @return the total size of bytes encoded in diff
 * 
 * diff[]:
 * | cache page addr (8bytes) | size of all elements in diff[] (4bytes) |  (start,size) 4bytes | cnt bytes different data |
 */
int encodediff(int cachei, unsigned char* diff)
{
  int size;
  int bytei;
  int cnt;
  int start;
  int header;

#ifdef DOSTAT
  register unsigned int begin = get_usecs();
#endif

  size=0;
  memcpy(diff+size,ltos(cache[cachei].addr),sizeof(unsigned char *));  // step 1: encode the cache page addr first (4 bytes)
  size+=sizeof(unsigned char *);
  size+=Intbytes;                       /* leave space for size */

  bytei=0;
  while (bytei<Pagesize) {
    for (; (bytei<Pagesize)&&(memcmp(cache[cachei].addr+bytei,
          cache[cachei].twin+bytei,Diffunit)==0); bytei+=Diffunit); // find the diff
    if (bytei<Pagesize) {  // here we got the difference between cache page and its twin
      cnt = 0;
      start = bytei; // record the start byte index of the diff
      for (; (bytei<Pagesize)&&(memcmp(cache[cachei].addr+bytei,
            cache[cachei].twin+bytei,Diffunit)!=0); bytei+=Diffunit) // how much diffunit is different
        cnt+=Diffunit;
      header=((start & 0xffff)<<16)|(cnt & 0xffff); // header is composed of start and cnt(diff size)
      memcpy(diff+size,ltos(header),Intbytes); // step 2: encode the header
      size+=Intbytes;
      memcpy(diff+size,cache[cachei].addr+start,cnt); // step 3: encode cnt different bytes
      size+=cnt;   
    }
  }
  memcpy(diff+sizeof(unsigned char *),ltos(size),Intbytes);    // step 4: fill the size

#ifdef DOSTAT
if (statflag==1){
  jiastat.endifftime += get_usecs() - begin;
}
#endif
  return(size);
}

/**
 * @brief savediff() -- save the difference between cached page(cachei) and its twin
 * 
 * @param cachei cache page index
 * 
 * If the diffsize + diffmsg[hosti] > Maxmsgsize, asendmsg; other, append the diff to the diffmsg[hosti]
 * hosti is the cached page's home host
 */
void savediff(int cachei)
{
  unsigned char  diff[Maxmsgsize];  // msg data array to store diff
  int   diffsize;
  int   hosti;
    
  hosti = homehost(cache[cachei].addr);   // according to cachei addr get the page's home host index 
  if (diffmsg[hosti] == DIFFNULL) { // hosti host's diffmsg is NULL 
    diffmsg[hosti] = newmsg();
    diffmsg[hosti]->op = DIFF;
    diffmsg[hosti]->frompid = jia_pid; 
    diffmsg[hosti]->topid = hosti; 
    diffmsg[hosti]->size = 0; 
  }
  diffsize=encodediff(cachei, diff);  // encoded the difference data between cachei page and its twin into diff [] and return size
  if ((diffmsg[hosti]->size+diffsize) > Maxmsgsize) {
    diffwait++;
    asendmsg(diffmsg[hosti]);
    diffmsg[hosti]->size=0;
    appendmsg(diffmsg[hosti],diff,diffsize);
    while(diffwait);
  }else{
    appendmsg(diffmsg[hosti],diff,diffsize);
  }
}

/**
 * @brief senddiffs() -- send msg in diffmsg[hosti] to correponding hosti host
 * 
 * 
 */
void senddiffs()
{
  int hosti;
 
  for (hosti = 0; hosti < hostc; hosti++) {
    if (diffmsg[hosti] != DIFFNULL) {  // hosti's diff msgs is non-NULL
      if (diffmsg[hosti]->size > 0) {  // diff data size > 0
        diffwait++;
        asendmsg(diffmsg[hosti]);    // asynchronous send diff msg
      }
      freemsg(diffmsg[hosti]);
      diffmsg[hosti]=DIFFNULL;
    }
 }
/*
 while(diffwait);
*/
}


void diffgrantserver(jia_msg_t *rep)
{
  assert((rep->op==DIFFGRANT)&&(rep->size==0),"Incorrect returned message!");

  diffwait--;
}


#else  /* NULL_LIB */

unsigned long jia_alloc(int size)
{
   return (unsigned long) valloc(size);
}
#endif /* NULL_LIB */
