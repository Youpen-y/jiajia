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
#include "init.h"
#include "global.h"
#include "jia.h"
#include "mem.h"
#include "setting.h"
#include "stat.h"
#include "tools.h"
#include "utils.h"
#include <libgen.h>


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
extern char errstr[Linesize];
extern short start_port;

static void copyfiles(int argc, char **argv);
static int startprocs(int argc, char **argv);
static void jiacreat(int argc, char **argv);
static void createdir(int argc, char **argv);
static void redirect_slave_io(int argc, char **argv);
static void barrier0();

int jia_lock_index;

/**
 * @brief jia_init -- init jiajia basic setting
 *
 * @param argc same as main
 * @param argv same as main
 */
void jia_init(int argc, char **argv) {

    // step 1:init system_setting
    init_setting(&system_setting);
    if (system_setting.jia_pid == 0) {
        print_setting(&system_setting);
        VERBOSE_LOG(3, "\n***JIAJIA---Software DSM***");
        VERBOSE_LOG(3, "\n***Cachepages = %4d  Pagesize=%d***\n\n", Cachepages,
                    Pagesize);
    }

    // step 2:start proc on slave host
    jia_lock_index = 0;
    jiacreat(argc, argv);

    // step 3:set resources' limit
    struct rlimit rl;
#if defined SOLARIS || defined LINUX
    rl.rlim_cur = Maxfileno;
    rl.rlim_max = Maxfileno;
    setrlimit(RLIMIT_NOFILE, &rl); /* set maximum number of files that can be
                                      opened by process limit */
#endif                             /* SOLARIS */
    rl.rlim_cur = Maxmemsize;
    rl.rlim_max = Maxmemsize;
    setrlimit(RLIMIT_DATA,
              &rl); /* set maximum size of process's data segment */

    // step 4:redirect slave's io (stdout&&stderr)
    if (system_setting.jia_pid != 0)
        redirect_slave_io(argc, argv); /*redirect slave's output*/

#ifdef DEBUG
    setbuf(logfile, NULL);
#endif

    // step 5:init system resources
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
#endif

    if (system_setting.jia_pid == 0)
        VERBOSE_LOG(3, "End of Initialization\n");
}

/**
 * @brief createdir -- create work dir jiajia/program/ in slaves
 *
 * @param argc same as main's argc
 * @param argv same as main's argv
 */
static void createdir(int argc, char **argv) {
    char cmd[Linesize];

    for (int i = 1; i < system_setting.hostc; i++) {
        sprintf(cmd,
                "ssh %s@%s \"[ ! -d 'jianode/%s' ] && mkdir -p 'jianode/%s'\"",
                system_setting.hosts[i].username, system_setting.hosts[i].ip,
                basename(argv[0]), argv[0]);
        system(cmd);
    }
}

/**
 * @brief copyfiles -- copy .jiahosts and program(i.e. argv[0]) to slaves
 *
 * @param argc same as main's argc
 * @param argv same as main's argv
 */
static void copyfiles(int argc, char **argv) {
    int i, ret;
    char cmd[Linesize];

    VERBOSE_LOG(3, "Start to copy system files to slaves!\n");

    // copy necessary files to slaves
    for (i = 1; i < system_setting.hostc; i++) {
        VERBOSE_LOG(3, "Copy files to %s@%s.\n",
                    system_setting.hosts[i].username,
                    system_setting.hosts[i].ip);

        sprintf(cmd, "scp .jiahosts system.conf %s %s@%s:~/jianode/%s",
                basename(argv[0]), system_setting.hosts[i].username,
                system_setting.hosts[i].ip, basename(argv[0]));
        ret = system(cmd);
        local_assert(ret == 0, "Copy system files failed");
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
static int startprocs(int argc, char **argv) {
    struct servent *sp;
    int hosti;
    char cmd[Linesize], *hostname;
    char shell[Linesize];

#ifdef NFS
    char *pwd;
#endif /* NFS*/

    VERBOSE_LOG(3, "Start to create processes on slaves!\n\n");

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

        hostname = system_setting.hosts[hosti].ip;

#ifdef NFS
        sprintf(cmd, "cd %s; %s", pwd, pwd);
        sprintf(cmd, "%s%s", cmd, hostname);
#else
        /* ssh username@ip (argv) -P 1234 & */
        cmd[0] = '\0';
        sprintf(cmd, "ssh %s@", system_setting.hosts[hosti].username);
        hostname = system_setting.hosts[hosti].ip;
        sprintf(cmd, "%s%s", cmd, hostname);
#endif /* NFS */

        /**
         * step 1: give program basename from argv[0]
         * step 2: use other argvs for remote host
         * step 3: use -P to ensure startport for commManager
         */
        char *base = basename(argv[0]);
        // sprintf(shell, "cd ./jianode/%s &&", base);
        // sprintf(shell, "%s ./%s", shell, base);
        sprintf(shell, "~/jianode/%s/%s", base, base);
        for (int i = 1; i < argc; i++) {
            sprintf(cmd, "%s %s", cmd, argv[i]);
        }
        sprintf(shell, "%s -P %ud &", shell, start_port);

        // strcat cmd && shell to execute
        sprintf(cmd, "%s '%s'", cmd, shell);
        VERBOSE_LOG(3, "Starting CMD %s on host %s\n", cmd, hostname);

        // bash cmd
        system(cmd);
        local_assert((system_setting.hosts[hosti].riofd != -1),
                     "Fail to start process on %s!",
                     system_setting.hosts[hosti].username);
    }

#else /*LINUX*/
    for (hosti = 1; hosti < system_setting.hostc; hosti++) {

#ifdef NFS
        sprintf(cmd, "cd %s; %s", pwd, pwd);
#else  /* NFS */
        cmd[0] = '\0';
        strcat(cmd, "~");
        strcat(cmd, system_setting.hosts[hosti].username);
#endif /* NFS */

#ifdef NFS
        system_setting.hosts[hosti].riofd =
            rexec(&hostname, sp->s_port, NULL, NULL, cmd,
                  &(system_setting.hosts[hosti].rerrfd));
#else  /* NFS */
        system_setting.hosts[hosti].riofd = rexec(
            &hostname, sp->s_port, system_setting.hosts[hosti].username,
            hosts[hosti].password, cmd,
            &(system_setting.startprocs(argc, argv);
              hosts[hosti].rerrfd)); // TODO rexec is obsoleted by rcmd (reason:
                                     // rexec sends the unencrypted password
                                     // across the network)
#endif /* NFS */
        local_assert((system_setting.hosts[hosti].riofd != -1),
                     "Fail to start process on %s!",
                     system_setting.hosts[hosti].username);
    }
#endif /* LINUX */

    return 0;
}

/**
 * @brief jiacreat -- creat process on other machines
 *
 * @param argc same as masters'
 * @param argv same as masters'
 */
static void jiacreat(int argc, char **argv) {
    if (system_setting.hostc == 0) {
        VERBOSE_LOG(3, "  No hosts specified!\n");
        exit(0);
    }

    if (system_setting.jia_pid == 0) { // master does
        VERBOSE_LOG(3, "*********Total of %d hosts found!**********\n\n",
                    system_setting.hostc);
#ifndef NFS
        /* step 1: create directories */
        createdir(argc, argv);

        /* step 2: copy files */
        copyfiles(argc, argv);
#endif /* NFS */

        /* step 3: start proc on slaves */
        startprocs(argc, argv);
    } else { // slave does
        int c;
        optind = 1;
        int i = 0;
        while ((c = getopt(argc, argv, "P:")) != -1) {
            switch (c) {
            case 'P':
                start_port = atol(optarg);
                break;
            }
        }
        optind = 1;
    }

    open_logfile("jiajia.log", argc, argv);
}

static void barrier0() {
    int hosti;
    char buf[4];

    if (system_setting.jia_pid == 0) {
        for (hosti = 1; hosti < system_setting.hostc; hosti++) {
            VERBOSE_LOG(3, "Poll host %d: stream %4d----", hosti,
                        system_setting.hosts[hosti].riofd);
            read(system_setting.hosts[hosti].riofd, buf, 3);
            buf[3] = '\0';
            VERBOSE_LOG(3, "%s Host %4d arrives!\n", buf, hosti);
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
static void redirect_slave_io(int argc, char **argv) {
    char outfile[Wordsize];

#ifdef NFS
    sprintf(outfile, "%s-%d.log\0", argv[0], system_setting.jia_pid);
#else
    sprintf(outfile, "%s.log", argv[0]);
#endif                             /* NFS */
    freopen(outfile, "w", stdout); // redirect stdout to file outfile
#ifdef DEBUG
    setbuf(stdout, NULL);
#endif

#ifdef NFS
    sprintf(outfile, "%s-%d.err\0", argv[0], system_setting.jia_pid);
#else
    sprintf(outfile, "%s.err", argv[0]);
#endif                             /* NFS */
    freopen(outfile, "w", stderr); // redirect stderr to file outfile
#ifdef DEBUG
    setbuf(stderr, NULL);
#endif
}

#else /* NULL_LIB */

#include <stdio.h>

system_setting.jia_pid = 0;
system_setting.hostc = 1;

void jia_init(int argc, char **argv) {
    VERBOSE_LOG(3, "This is JIAJIA-NULL\n");
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
