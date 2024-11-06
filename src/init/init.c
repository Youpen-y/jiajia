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

#include "tools.h"
#ifndef NULL_LIB
#include "global.h"
#include "init.h"
#include "mem.h"
#include "utils.h"
#include "setting.h"
#include "stat.h"

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

extern unsigned int get_usecs();

// int my_getline(int *wordc, char wordv[Maxwords][Wordsize]);
// void gethosts();
// int mypid();
void copyfiles(int argc, char **argv);
int startprocs(int argc, char **argv);
void jiacreat(int argc, char **argv);
void barrier0();

void jia_init(int argc, char **argv);
void clearstat();

extern char errstr[Linesize];
extern long start_port;


sigset_t startup_mask;      /* used by Shi. */
int jia_lock_index;



/**
 * @brief copyfiles -- copy .jiahosts and program(i.e. argv[0]) to slaves
 *
 * @param argc same as main's argc
 * @param argv same as main's argv
 */
void copyfiles(int argc, char **argv) {
    // replace rcp with scp
    int hosti, rcpyes;
    char cmd[Linesize];

    VERBOSE_LOG(3, "******Start to copy system files to slaves!******\n");

    for (hosti = 1; hosti < system_setting.hostc; hosti++) {
        VERBOSE_LOG(3, "Copy files to %s@%s.\n", system_setting.hosts[hosti].username,
                    system_setting.hosts[hosti].ip);

        /* copy .jiahosts to slaves */
        cmd[0] = '\0';
        strcat(cmd, "scp .jiahosts ");
        strcat(cmd, system_setting.hosts[hosti].username);
        strcat(cmd, "@");
        strcat(cmd, system_setting.hosts[hosti].ip);
        strcat(cmd, ":");
        rcpyes = system(cmd);
        local_assert((rcpyes == 0), "Cannot scp .jiahosts to %s!\n",
                system_setting.hosts[hosti].ip);

        /* copy system.conf to slaves */
        cmd[0] = '\0';
        strcat(cmd, "scp system.conf ");
        strcat(cmd, system_setting.hosts[hosti].username);
        strcat(cmd, "@");
        strcat(cmd, system_setting.hosts[hosti].ip);
        strcat(cmd, ":");
        rcpyes = system(cmd);
        local_assert((rcpyes == 0), "Cannot scp system.conf to %s!\n",
                system_setting.hosts[hosti].ip);

        /* copy program to slaves */
        cmd[0] = '\0';
        strcat(cmd, "scp ");
        strcat(cmd, argv[0]);
        strcat(cmd, " ");
        strcat(cmd, system_setting.hosts[hosti].username);
        strcat(cmd, "@");
        strcat(cmd, system_setting.hosts[hosti].ip);
        strcat(cmd, ":");
        rcpyes = system(cmd);
        local_assert((rcpyes == 0), "Cannot scp %s to %s!\n", argv[0],
                system_setting.hosts[hosti].ip);
    }
    VERBOSE_LOG(3, "Remote copy succeed!\n\n");
}

/**
 * @brief startprocs -- start process on slaves
 *
 * @param argc same as masters'
 * @param argv same as masters'
 * @return int
 */
int startprocs(int argc, char **argv) {
    struct servent *sp;

#ifdef NFS
    char *pwd;
#endif /* NFS*/
    int hosti;
    char cmd[Linesize], *hostname;

    VERBOSE_LOG(3, "******Start to create processes on slaves!******\n\n");

#ifdef NFS
    sprintf(errstr, "Failed to get current working directory");
    pwd = getenv("PWD");
    local_assert((pwd != NULL), errstr);
#endif /* NFS */

    /* produce a random start_port from [10000, 29999]*/
    start_port = getpid();
    local_assert((start_port != -1), "getpid() error");
    start_port = 10000 + (start_port * Maxhosts * Maxhosts * 4) % 20000;

#ifdef LINUX
    // cmd on every host
    for (hosti = 1; hosti < system_setting.hostc; hosti++) {

        /* ssh -l username ip (argv) -P 1234 & */
        hostname = system_setting.hosts[hosti].ip;
#ifdef NFS
        sprintf(cmd, "cd %s; %s", pwd, pwd);
#else
        cmd[0] = '\0';
        sprintf(cmd, "ssh -l %s", system_setting.hosts[hosti].username);
#endif /* NFS */
        sprintf(cmd, "%s %s", cmd, hostname);

        for (int i = 0; i < argc; i++)
            sprintf(cmd, "%s %s ", cmd, argv[i]);

        strcat(cmd, "-P");
        sprintf(cmd, "%s %ld", cmd, start_port);
        strcat(cmd, " &");
        VERBOSE_LOG(3, "Starting CMD %s on host %s\n", cmd, hostname);
        system(cmd);
#else /*LINUX*/
    for (hosti = 1; hosti < system_setting.hostc; hosti++) {
#ifdef NFS
        sprintf(cmd, "cd %s; %s", pwd, pwd);
#else  /* NFS */
        cmd[0] = '\0';
        strcat(cmd, "~");
        strcat(cmd, system_setting.hosts[hosti].username);
#endif /* NFS */
        strcat(cmd, "/");
        for (i = 0; i < argc; i++) {
            strcat(cmd, argv[i]);
            strcat(cmd, " ");
        }
        strcat(cmd, "-P");
        sprintf(cmd, "%s%d ", cmd, start_port);

        VERBOSE_LOG(3,"Starting CMD %s on host %s\n", cmd, system_setting.hosts[hosti].ip);
        sp = getservbyname("exec", "tcp");
        local_assert((sp != NULL), "exec/tcp: unknown service!");
        hostname = system_setting.hosts[hosti].username;

#ifdef NFS
        system_setting.hosts[hosti].riofd = rexec(&hostname, sp->s_port, NULL, NULL, cmd,
                                   &(system_setting.hosts[hosti].rerrfd));
#else  /* NFS */
        system_setting.hosts[hosti].riofd = rexec(
            &hostname, sp->s_port, system_setting.hosts[hosti].username, hosts[hosti].password, cmd,
            &(system_setting.hosts[hosti].rerrfd)); // TODO rexec is obsoleted by rcmd (reason:
                                     // rexec sends the unencrypted password
                                     // across the network)
#endif /* NFS */
#endif /* LINUX */
        local_assert((system_setting.hosts[hosti].riofd != -1), "Fail to start process on %s!",
                system_setting.hosts[hosti].username);
    }

    return 0;
}

/**
 * @brief jiacreat --
 *
 * @param argc
 * @param argv
 */
void jiacreat(int argc, char **argv) {
    if (system_setting.hostc == 0) {
        VERBOSE_LOG(3, "  No hosts specified!\n");
        exit(0);
    }

    if (system_setting.jia_pid == 0) {
        // master does
        VERBOSE_LOG(3, "*********Total of %d hosts found!**********\n\n",
                system_setting.hostc);
#ifndef NFS
        copyfiles(argc, argv);
#endif /* NFS */
        sleep(1);

        // step 4: start proc on slaves
        startprocs(argc, argv);
    } else {
        // slave does
        int c;
        optind = 1;
        int i=0;
        while ((c = getopt(argc, argv, "P:")) != -1) {
            switch (c) {
            case 'P': {
                start_port = atol(optarg);
                break;
            }
            }
        }
        optind = 1;
    }
}

void barrier0() {
    int hosti;
    char buf[4];

    if (system_setting.jia_pid == 0) {
        for (hosti = 1; hosti < system_setting.hostc; hosti++) {
            VERBOSE_LOG(3,"Poll host %d: stream %4d----", hosti, system_setting.hosts[hosti].riofd);
            read(system_setting.hosts[hosti].riofd, buf, 3);
            buf[3] = '\0';
            VERBOSE_LOG(3,"%s Host %4d arrives!\n", buf, hosti);
#ifdef NFS
            write(system_setting.hosts[hosti].riofd, "ok!", 3);
#endif
        }
#ifndef NFS
        for (hosti = 1; hosti < system_setting.hostc; hosti++)
            write(system_setting.hosts[hosti].riofd, "ok!", 3);
#endif
    } else {
        write(1, "ok?", 3);
        read(0, buf, 3);
    }
}

/**
 * @brief redirstdio -- redirect standard I/O to file (stdout -- argv[0].log
 * stderr -- argv[0].err)
 *
 * @param argc program argument count
 * @param argv program arguments array
 * @note redirstdio makes effects on slaves only
 */

void redirect_slave_io(int argc, char **argv) {
    char outfile[Wordsize];

    if (system_setting.jia_pid != 0) { // slaves does
#ifdef NFS
        sprintf(outfile, "%s-%d.log\0", argv[0], system_setting.jia_pid);
#else
        sprintf(outfile, "%s.log\0", argv[0]);
#endif                                 /* NFS */
        freopen(outfile, "w", stdout); // redirect stdout to file outfile
        setbuf(stdout, NULL);

#ifdef NFS
        sprintf(outfile, "%s-%d.err\0", argv[0], system_setting.jia_pid);
#else
        sprintf(outfile, "%s.err\0", argv[0]);
#endif                                 /* NFS */
        freopen(outfile, "w", stderr); // redirect stderr to file outfile
        setbuf(stderr, NULL);          
    }
}

/**
 * @brief jia_init -- init jiajia basic setting
 *
 * @param argc same as main
 * @param argv same as main
 */
void jia_init(int argc, char **argv) {
    init_setting(&system_setting);
    open_logfile("jiajia.log");
    if (system_setting.jia_pid == 0) {
        print_setting(&system_setting);
    }

    unsigned long timel, time1;
    struct rlimit rl;

    VERBOSE_LOG(3, "\n***JIAJIA---Software DSM***\n***  \
                Cachepages = %4d  Pagesize=%d***\n\n",
                Cachepages, Pagesize);
    disable_sigio();
    jia_lock_index = 0;
    jiacreat(argc, argv);
#if defined SOLARIS || defined LINUX
    sleep(2);
    rl.rlim_cur = Maxfileno;
    rl.rlim_max = Maxfileno;
    setrlimit(RLIMIT_NOFILE, &rl); /* set maximum number of files that can be
                                      opened by process limit */
#endif                             /* SOLARIS */

    rl.rlim_cur = Maxmemsize;
    rl.rlim_max = Maxmemsize;
    setrlimit(RLIMIT_DATA,
              &rl); /* set maximum size of process's data segment */

    redirect_slave_io(argc, argv); /*redirect slave's output*/

    initmem();
    initsyn();
    initcomm();
    initmsg();
    inittools();
    initload();

#ifdef DOSTAT
    clearstat();
    statflag = 1; // stat switch on
#endif
#ifndef LINUX
    barrier0();
#else
    sleep(2);
#endif

    if (system_setting.jia_pid != 0) { // slave does
        VERBOSE_LOG(3, "I am %d, running here\n", system_setting.jia_pid);
    }
    enable_sigio();

    timel = jia_current_time();
    time1 = jia_clock();

    if (system_setting.jia_pid == 0)
        VERBOSE_LOG(3,"End of Initialization\n");

    if (system_setting.jia_pid != 0)
        sleep(1);
}

#else /* NULL_LIB */

#include <stdio.h>

system_setting.jia_pid = 0;
system_setting.hostc = 1;

void jia_init(int argc, char **argv) {
    VERBOSE_LOG(3,"This is JIAJIA-NULL\n");
}
#endif /* NULL_LIB */

unsigned int t_start, t_stop = 0;

unsigned int jia_startstat() {
#ifdef DOSTAT
    clearstat();
    statflag = 1;
#endif
    t_start = get_usecs();
    return t_start;
}

unsigned int jia_stopstat() {
#ifdef DOSTAT
    statflag = 0;
#endif
    t_stop = get_usecs();
    return t_stop;
}
