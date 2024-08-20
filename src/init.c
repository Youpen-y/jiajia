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
 *           Author: Weiwu Hu, Weisong Shi, Zhimin Tang                *
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
#include "init.h"
#include "mem.h"

extern void initmem();
extern void initsyn();
extern void initcomm();
extern void initmsg();
extern void inittools();
extern void initload();
extern void disable_sigio();         
extern void enable_sigio();       
extern unsigned long jia_current_time();
extern float jia_clock();

extern void assert0(int, char *);
extern unsigned int get_usecs();

int my_getline(int *wordc, char wordv[Maxwords][Wordsize]);
void gethosts();
int  mypid();
void copyfiles(int argc, char **argv);
int startprocs(int argc, char **argv);
void jiacreat(int argc, char **argv);
void barrier0();
void redirstdio(int argc, char **argv);
void jia_init(int argc, char **argv);
void clearstat();


extern char errstr[Linesize];
extern long Startport;

FILE  *config, *fopen();
int   jia_pid;                       /* node number */
host_t hosts[Maxhosts+1];            /* host array */
int  hostc;                          /* host counter */
char   argv0[Wordsize];              /* program name */
sigset_t startup_mask;               /* used by Shi. */
int jia_lock_index;

#ifdef DOSTAT
jiastat_t jiastat;
int statflag;
#endif

/**
 * @brief my_getline -- 
 * 
 * @param wordc 
 * @param wordv 
 * @return int 
 */
int my_getline(int *wordc, char wordv[Maxwords][Wordsize])
{
  char line[Linesize];
  int ch;
  int linei,wordi1,wordi2;
  int note;

  linei=0;
  note=0;
  ch=getc(config);
  if (ch=='#') note=1;
  while ((ch!='\n')&&(ch!=EOF)){
    if ((linei<Linesize-1)&&(note==0)){
      line[linei]=ch;
      linei++;
    }
    ch=getc(config);
    if (ch=='#') note=1;
  }
  line[linei]='\0';

  for (wordi1=0;wordi1<Maxwords;wordi1++)
    wordv[wordi1][0]='\0';
  wordi1=0; linei=0;
  while ((line[linei]!='\0')&&(wordi1<Maxwords)){
    while ((line[linei]==' ')||(line[linei]=='\t'))  
      linei++;
    wordi2=0;
    while ((line[linei]!=' ')&&(line[linei]!='\t')&&(line[linei]!='\0')){
      if (wordi2<Wordsize-1){
        wordv[wordi1][wordi2]=line[linei];
        wordi2++;
      }
      linei++;
    }
    if (wordi2>0){ 
      wordv[wordi1][wordi2]='\0';  
      wordi1++;
    }
  }

  *wordc=wordi1;
  return(ch==EOF);
}

/**
 * @brief gethosts -- open .jiahosts and according its contents fill the hosts array
 * 
 */
void gethosts()
{
  int endoffile;
  int  wordc, linec, uniquehost;
  char wordv[Maxwords][Wordsize];
  struct hostent *hostp;
  int i;

  if ((config=fopen(".jiahosts","r")) == 0) {
    printf("Cannot open .jiahosts file\n");
    exit(1);
  }

  endoffile=0;
  hostc=0;    // initial host number is 0
  linec=0;
  while(!endoffile){
    endoffile=my_getline(&wordc,wordv);
    linec++;
    sprintf(errstr,"Line %4d: incorrect host specification!",linec);
    assert0(((wordc==Wordnum)||(wordc==0)),errstr);
    if (wordc!=0){
      hostp=gethostbyname(wordv[0]);  // TODO gethostbyname() has been obsolete, use getaddrinfo, get-nameinfo, gai_strerror instead
      printf("Host[%d]: %s [%s]\n", hostc, hostp->h_name, 
             inet_ntoa(*(struct in_addr*)hostp->h_addr_list[0]));
      sprintf(errstr,"Line %4d: incorrect host %s!",linec,wordv[0]);
      assert0((hostp!=NULL),errstr);
      strcpy(hosts[hostc].name,hostp->h_name); 
      memcpy(hosts[hostc].addr,hostp->h_addr,hostp->h_length);
      hosts[hostc].addrlen=hostp->h_length;
      strcpy(hosts[hostc].user,wordv[1]); 
      strcpy(hosts[hostc].passwd,wordv[2]);

      for (i=0;i<hostc;i++){
#ifdef NFS
       uniquehost=(strcmp(hosts[hostc].name,hosts[i].name)!=0);
#else  /* NFS */
      uniquehost=((strcmp(hosts[hostc].addr,hosts[i].addr)!=0)||
                   (strcmp(hosts[hostc].user,hosts[i].user)!=0));
#endif /*NFS */
       sprintf(errstr,"Line %4d: repeated specification of the same host!",linec);
       assert0(uniquehost,errstr);
     }
      hostc++; 
   } 
 }

  assert0((hostc<=Maxhosts),"Too many hosts!");
  fclose(config);
}

/**
 * @brief copyfiles -- copy .jiahosts and program(i.e. argv[0]) to slaves
 * 
 * @param argc same as main's argc
 * @param argv same as main's argv
 */
void copyfiles(int argc, char **argv)
{
  // replace rcp with scp
  int hosti,rcpyes;
  char cmd[Linesize];

  printf("******Start to copy system files to slaves!******\n");

  for (hosti=1;hosti<hostc;hosti++){
    printf("Copy files to %s@%s.\n",hosts[hosti].user,hosts[hosti].name);

    cmd[0]='\0';
    // strcat(cmd,"rcp .jiahosts ");
    strcat(cmd,"scp .jiahosts ");
    strcat(cmd,hosts[hosti].user);
    strcat(cmd,"@");
    strcat(cmd,hosts[hosti].name);
    strcat(cmd,":");

    rcpyes=system(cmd);
    sprintf(errstr,"Cannot scp .jiahosts to %s!\n",hosts[hosti].name);
    assert0((rcpyes==0), errstr);

    /* copy program to slaves */
    cmd[0]='\0';
    strcat(cmd,"scp ");
    strcat(cmd,argv[0]);
    strcat(cmd," ");
    strcat(cmd,hosts[hosti].user);
    strcat(cmd,"@");
    strcat(cmd,hosts[hosti].name);
    strcat(cmd,":");
    rcpyes=system(cmd);
    sprintf(errstr,"Cannot scp %s to %s!\n",argv[0],hosts[hosti].name);
    assert0((rcpyes==0),errstr);
  } 
  printf("Remote copy succeed!\n\n");
}

/**
 * @brief startprocs -- start process on slaves
 * 
 * @param argc 
 * @param argv 
 * @return int 
 */
int startprocs(int argc, char **argv)
{
  struct servent *sp;
  
#ifdef NFS
  char *pwd;
#endif /* NFS*/
  int hosti;
  char cmd[Linesize], *hostname;
  int i;
  
  printf("******Start to create processes on slaves!******\n\n");

#ifdef NFS
  sprintf(errstr,"Failed to get current working directory");
  pwd = getenv("PWD"); 
  assert0((pwd != NULL),errstr); 
#endif /* NFS */

  /* produce a random Startport from [10000, 29999]*/
  Startport = getpid();
  assert0((Startport != -1), "getpid() error");
  Startport = 10000+(Startport*Maxhosts*Maxhosts*4)%20000;

#ifdef LINUX 
  for (hosti=1; hosti<hostc; hosti++) {
#ifdef NFS
    sprintf(cmd,"cd %s; %s", pwd, pwd);
#else  
    cmd[0]='\0';
    strcat(cmd,"ssh -l ");
    strcat(cmd, hosts[hosti].user);
#endif /* NFS */
    hostname = hosts[hosti].name;
    strcat(cmd, " ");
    strcat(cmd, hostname); 
    strcat(cmd, " ");
    for (i = 0; i < argc; i++){
      strcat(cmd, argv[i]);
      strcat(cmd, " ");
    }

    strcat(cmd, "-P");
    sprintf(cmd,"%s %ld", cmd, Startport);
    strcat(cmd, " &");
    printf("Starting CMD %s on host %s\n", cmd, hosts[hosti].name);
    system(cmd);
#else /*LINUX*/
  for (hosti=1;hosti<hostc;hosti++){
#ifdef NFS
    sprintf(cmd,"cd %s; %s", pwd, pwd);
#else  /* NFS */
    cmd[0]='\0';
    strcat(cmd,"~");
    strcat(cmd,hosts[hosti].user);
#endif /* NFS */
    strcat(cmd,"/");
    for (i=0;i<argc;i++){
      strcat(cmd,argv[i]);
      strcat(cmd," ");
    }
    strcat(cmd, "-P");
    sprintf(cmd,"%s%d ",cmd,Startport);

    printf("Starting CMD %s on host %s\n", cmd, hosts[hosti].name);
    sp=getservbyname("exec","tcp");
    assert0((sp!=NULL),"exec/tcp: unknown service!");
    hostname=hosts[hosti].name;

#ifdef NFS
    hosts[hosti].riofd=rexec(&hostname,sp->s_port, NULL, NULL,
                              cmd,&(hosts[hosti].rerrfd));
#else  /* NFS */
    hosts[hosti].riofd=rexec(&hostname,sp->s_port,hosts[hosti].user,
                              hosts[hosti].passwd,cmd,&(hosts[hosti].rerrfd));  // TODO rexec is obsoleted by rcmd
#endif /* NFS */
#endif /* LINUX */
    sprintf(errstr,"Fail to start process on %s!",hosts[hosti].name);
    assert0((hosts[hosti].riofd!=-1),errstr);

  }
}

/**
 * @brief mypid -- get host id [0, hostc)
 * 
 * @return int id number
 */
int mypid()
{
  char hostname[Wordsize];
  uid_t uid;
  struct passwd *userp;
  struct hostent *hostp;
  int i;
  
  assert0((gethostname(hostname,Wordsize)==0), "Cannot get host name!");
  hostp=gethostbyname(hostname);
  assert0((hostp!=NULL),"Cannot get host address!");

  uid=getuid();
  userp=getpwuid(uid);
  assert0((userp!=NULL),"Cannot get user name!");

  i=0;

  // test1
  printf("hostc = %d\n", hostc);
  strtok(hostname,".");
  printf("hostname = %s\n", hostname);
  while ((i<hostc) &&
#ifdef NFS
         (!(strncmp(hosts[i].name,hostname,strlen(hostname))==0)))
#else  /* NFS */
         (!((strncmp(hosts[i].name,hostname,strlen(hostname))==0)&&
            (strcmp(hosts[i].user,userp->pw_name)==0)))) 
#endif /* NFS */
    i++; 

  assert0((i<hostc),"Get Process id incorrect");
  return(i);
}

/**
 * @brief jiacreat -- 
 * 
 * @param argc 
 * @param argv 
 */
void jiacreat(int argc, char **argv)
{
  gethosts();
  if (hostc==0) {
    printf("  No hosts specified!\n");
    exit(0);
  }
  jia_pid=mypid();

  if (jia_pid==0){ // master does, slave doesn't
    printf("*********Total of %d hosts found!**********\n\n",hostc);
#ifndef NFS
      copyfiles(argc,argv); 
#endif /* NFS */
    sleep(1);
    startprocs(argc,argv);
  } else {  // slave does
    int c;
    optind=1;
    while ((c = getopt(argc, argv, "P:")) != -1){
      switch (c) {
        case 'P':{
          Startport = atol(optarg);
          break;
        }
      }
    }
    optind=1;
  }
}


void barrier0()
{
  int hosti;
  char buf[4];

  if (jia_pid==0){
    for (hosti=1;hosti<hostc;hosti++){
      printf("Poll host %d: stream %4d----", hosti, hosts[hosti].riofd);
      read(hosts[hosti].riofd,buf,3);
      buf[3]='\0';
      printf("%s Host %4d arrives!\n",buf,hosti);
      #ifdef NFS
        write(hosts[hosti].riofd,"ok!",3);
      #endif
    }
    #ifndef NFS
      for (hosti=1;hosti<hostc;hosti++)
      write(hosts[hosti].riofd,"ok!",3);
    #endif
  }else{
    write(1,"ok?",3);
    read(0,buf,3);
  }
}


/**
 * @brief redirstdio -- redirect standard I/O to file (stdout -- argv[0].log stderr -- argv[0].err)
 * 
 * @param argc program argument count
 * @param argv program arguments array
 * @note redirstdio makes effects on slaves only
 */

void redirstdio(int argc, char **argv)
{
  char outfile[Wordsize];
 
  if (jia_pid!=0){  // slaves does
    #ifdef NFS
      sprintf(outfile,"%s-%d.log\0",argv[0],jia_pid);
    #else
      sprintf(outfile,"%s.log\0",argv[0]);
    #endif /* NFS */
    freopen(outfile,"w",stdout);  // redirect stdout to file outfile
    setbuf(stdout,NULL);

    #ifdef NFS
      sprintf(outfile,"%s-%d.err\0",argv[0],jia_pid);
    #else
      sprintf(outfile,"%s.err\0",argv[0]);
    #endif /* NFS */
    freopen(outfile,"w",stderr);  // redirect stderr to file outfile
    setbuf(stderr,NULL);  //
  }
}


void jia_init(int argc, char **argv)
{
  unsigned long timel,time1;
  struct rlimit rl;

  printf("\n***JIAJIA---Software DSM***\n");
  printf("***  Cachepages = %4d  Pagesize=%d***\n\n", Cachepages,Pagesize);
  strcpy(argv0, argv[0]);
  disable_sigio();
  jia_lock_index=0;  
  jiacreat(argc, argv);
#if defined SOLARIS || defined LINUX
  sleep(2);
  rl.rlim_cur=Maxfileno;
  rl.rlim_max=Maxfileno;
  setrlimit(RLIMIT_NOFILE,&rl);   /* set maximum number of files that can be opened by process limit */
#endif /* SOLARIS */

  rl.rlim_cur=Maxmemsize;
  rl.rlim_max=Maxmemsize;
  setrlimit(RLIMIT_DATA,&rl);   /* set maximum size of process's data segment */
  
  initmem();
  initsyn();
  initcomm();
  initmsg();
  inittools();
  initload();

#ifdef DOSTAT
  clearstat();
  statflag=1;
#endif
#ifndef LINUX
  barrier0();
#else
  sleep(2);
#endif
  redirstdio(argc,argv);

  if(jia_pid != 0){ // slave does
    printf("I am %d, running here\n", jia_pid);
  }
  enable_sigio();

  timel=jia_current_time();
  time1=jia_clock();

  if (jia_pid==0)
    printf ("End of Initialization\n");

  if (jia_pid!=0) sleep(1);
}

#ifdef DOSTAT
void clearstat()
{
  memset((char*)&jiastat, 0,sizeof(jiastat));
}
#endif

#else  /* NULL_LIB */

#include <stdio.h>

int   jia_pid = 0; 
int   hostc = 1;

void jia_init(int argc, char **argv)
{
  printf("This is JIAJIA-NULL\n");
}
#endif /* NULL_LIB */

unsigned int t_start, t_stop=0;

unsigned int jia_startstat()
{
#ifdef DOSTAT
   clearstat();
   statflag=1;
#endif
   t_start = get_usecs();
   return t_start;
}

unsigned int jia_stopstat()
{
#ifdef DOSTAT
   statflag=0;
#endif
   t_stop = get_usecs();
   return t_stop;
}
