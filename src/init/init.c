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
#include <ifaddrs.h>
#ifndef NULL_LIB
#include "global.h"
#include "init.h"
#include "mem.h"
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

void createdir(int argc, char **argv);
void gethosts();
int mypid();
void copyfiles(int argc, char **argv);
int startprocs(int argc, char **argv);
void jiacreat(int argc, char **argv);
void barrier0();
void redirstdio(int argc, char **argv);
void jia_init(int argc, char **argv);
void clearstat();

extern char errstr[Linesize];
extern long Startport;

FILE *config, *fopen();
FILE *logfile;
int jia_pid;                /* node number */
host_t hosts[Maxhosts + 1]; /* host array */
int hostc;                  /* host counter */
char argv0[Wordsize];       /* program name */
sigset_t startup_mask;      /* used by Shi. */
int jia_lock_index;

#ifdef DOSTAT
jiastat_t jiastat;
int statflag;
#endif

/**
 * @brief gethosts -- open .jiahosts and according its contents fill the hosts
 * array
 *
 */
void gethosts() {
    FILE *fp;
    fp = fopen(".jiahosts", "r");
    if (fp == NULL) {
        VERBOSE_LOG(3, "Cannot open.jiahosts file\n");
        exit(1);
    }

    char line[Linesize];
    hostc = 0;
    while (fgets(line, Linesize, fp)) {
        // remove '\n' at the end of line
        line[strcspn(line, "\n")] = 0;

        // skip empty lines and comments
        if (strlen(line) == 1 || line[0] == '\0' || line[0] == '#') {
            continue;
        }

        // parse the line into the host structure
        if (sscanf(line, "%15[0-9.] %31[^ ] %31[^ ]",
                   hosts[hostc].ip,
                   hosts[hostc].username,
                   hosts[hostc].password) == 3) {
            hosts[hostc].id = hostc;
            hostc++;
        } else {
            fprintf(stderr, "func-get_hosts: invalid line: %s\n", line);
        }

        // check if the hostc exceeds the Maxhosts
        if (hostc >= Maxhosts) {
            fprintf(stderr, "func-get_hosts: hostc exceeds the Maxhosts!\n");
            break;
        }
    }

    fclose(fp);
}

void createdir(int argc, char **argv) {
    char cmd[Linesize];

    for (int i = 1; i < hostc; i++) {
        sprintf(cmd,
                "ssh %s@%s \"[ ! -d 'jianode/%s' ] && mkdir -p 'jianode/%s'\"",
                hosts[i].username, hosts[i].ip,
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
void copyfiles(int argc, char **argv) {
    int i, ret;
    char cmd[Linesize];

    VERBOSE_LOG(3, "Start to copy system files to slaves!\n");

    // copy necessary files to slaves
    for (i = 1; i < hostc; i++) {
        VERBOSE_LOG(3, "Copy files to %s@%s.\n",
                    hosts[i].username,
                    hosts[i].ip);

        sprintf(cmd, "scp .jiahosts %s@%s:~/",
                hosts[i].username, hosts[i].ip);
        ret = system(cmd);
        sprintf(cmd, "scp %s %s@%s:~/jianode/%s/", basename(argv[0]),
                hosts[i].username, hosts[i].ip,
                basename(argv[0]));
        ret = system(cmd);
        assert0(ret == 0, "Copy system files failed");
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
    int pid = getpid();
    assert0((pid != -1), "getpid() error");
    Startport = 10000 + (pid % 20000);

#ifdef LINUX
    // cmd on every host
    for (hosti = 1; hosti < hostc; hosti++) {

        hostname = hosts[hosti].ip;

#ifdef NFS
        sprintf(cmd, "cd %s; %s", pwd, pwd);
        sprintf(cmd, "%s%s", cmd, hostname);
#else
        /* ssh username@ip (argv) -P 1234 & */
        cmd[0] = '\0';
        sprintf(cmd, "ssh %s@", hosts[hosti].username);
        hostname = hosts[hosti].ip;
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
        sprintf(shell, "%s -P %ld &", shell, Startport);

        // strcat cmd && shell to execute
        sprintf(cmd, "%s '%s'", cmd, shell);
        VERBOSE_LOG(3, "Starting CMD %s on host %s\n", cmd, hostname);

        // bash cmd
        system(cmd);
        assert0((hosts[hosti].riofd != -1),
                     "Fail to start process on %s!",
                     hosts[hosti].username);
    }

#else /*LINUX*/
    for (hosti = 1; hosti < hostc; hosti++) {

#ifdef NFS
        sprintf(cmd, "cd %s; %s", pwd, pwd);
#else  /* NFS */
        cmd[0] = '\0';
        strcat(cmd, "~");
        strcat(cmd, hosts[hosti].username);
#endif /* NFS */

#ifdef NFS
        hosts[hosti].riofd =
            rexec(&hostname, sp->s_port, NULL, NULL, cmd,
                  &(hosts[hosti].rerrfd));
#else  /* NFS */
        hosts[hosti].riofd = rexec(
            &hostname, sp->s_port, hosts[hosti].username,
            hosts[hosti].password, cmd,
            &(startprocs(argc, argv);
              hosts[hosti].rerrfd)); // TODO rexec is obsoleted by rcmd (reason:
                                     // rexec sends the unencrypted password
                                     // across the network)
#endif /* NFS */
        local_assert((hosts[hosti].riofd != -1),
                     "Fail to start process on %s!",
                     hosts[hosti].username);
    }
#endif /* LINUX */

    return 0;
}

/**
 * @brief mypid -- get host id [0, hostc)
 *
 * @return int id number
 */
int mypid() {
    struct ifaddrs *ifap, *ifa;
    int found = 0;
    int jia_pid;

    if (getifaddrs(&ifap) == -1) {
        fprintf(stderr, "getifaddrs: %s\n", strerror(errno));
        exit(1);
    }

    for (ifa = ifap; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL || ifa->ifa_addr->sa_family != AF_INET) {
            continue;
        }

        char ipstr[128];
        inet_ntop(ifa->ifa_addr->sa_family,
                  &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr, ipstr,
                  sizeof ipstr);

        for (int i = 0; i < hostc; i++) {
            if (strcmp(hosts[i].ip, ipstr) == 0) {
                jia_pid = i;
                found = 1;
                break;
            }
        }
        if (found == 1) {
            break;
        }
    }

    freeifaddrs(ifap);
    return jia_pid;
}

/**
 * @brief jiacreat --
 *
 * @param argc
 * @param argv
 */
void jiacreat(int argc, char **argv) {
    logfile = fopen("./jiajia.log", "w+");

    // step 1: get hosts info
    gethosts();
    if (hostc == 0) {
        VERBOSE_LOG(3, "  No hosts specified!\n");
        exit(0);
    }

    // step 2: get current host's jia_pid
    jia_pid = mypid();
    if (jia_pid == 0) { // master does, slave doesn't
        VERBOSE_LOG(3, "*********Total of %d hosts found!**********\n\n",
                    hostc);

        // step 3: create dir on slaves
        createdir(argc, argv);

        // step 4: copy files to remote
#ifndef NFS
        copyfiles(argc, argv);
#endif /* NFS */

        // step 5: start proc on slaves
        startprocs(argc, argv);
    } else {
        // slave does
        int c;
        optind = 1;
        int i = 0;
        while ((c = getopt(argc, argv, "P:")) != -1) {
            switch (c) {
            case 'P': {
                Startport = atol(optarg);
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

    if (jia_pid == 0) {
        for (hosti = 1; hosti < hostc; hosti++) {
            VERBOSE_LOG(3, "Poll host %d: stream %4d----", hosti,
                        hosts[hosti].riofd);
            read(hosts[hosti].riofd, buf, 3);
            buf[3] = '\0';
            VERBOSE_LOG(3, "%s Host %4d arrives!\n", buf, hosti);
#ifdef NFS
            write(hosts[hosti].riofd, "ok!", 3);
#endif
        }
#ifndef NFS
        for (hosti = 1; hosti < hostc; hosti++)
            write(hosts[hosti].riofd, "ok!", 3);
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

void redirstdio(int argc, char **argv) {
    char outfile[Wordsize];

    if (jia_pid != 0) { // slaves does
#ifdef NFS
        sprintf(outfile, "%s-%d.log\0", argv[0], jia_pid);
#else
        sprintf(outfile, "%s.log\0", argv[0]);
#endif                                 /* NFS */
        freopen(outfile, "w", stdout); // redirect stdout to file outfile
        setbuf(stdout, NULL);

#ifdef NFS
        sprintf(outfile, "%s-%d.err\0", argv[0], jia_pid);
#else
        sprintf(outfile, "%s.err\0", argv[0]);
#endif                                 /* NFS */
        freopen(outfile, "w", stderr); // redirect stderr to file outfile
        setbuf(stderr, NULL);          //
    }
}

/**
 * @brief jia_init -- init jiajia basic setting
 *
 * @param argc
 * @param argv
 */
void jia_init(int argc, char **argv) {
    unsigned long timel, time1;
    struct rlimit rl;
    if (jia_pid == 0) {
        VERBOSE_LOG(3, "\n***JIAJIA---Software DSM***");
        VERBOSE_LOG(3, "\n***Cachepages = %4d  Pagesize=%d***\n",
                    Cachepages, Pagesize);
    }

    strcpy(argv0, argv[0]);
    disable_sigio();
    jia_lock_index = 0;
    jiacreat(argc, argv);
#if defined SOLARIS || defined LINUX
    rl.rlim_cur = Maxfileno;
    rl.rlim_max = Maxfileno;
    setrlimit(RLIMIT_NOFILE, &rl); /* set maximum number of files that can be
                                      opened by process limit */
#endif

    setbuf(logfile, NULL);

    rl.rlim_cur = Maxmemsize;
    rl.rlim_max = Maxmemsize;
    setrlimit(RLIMIT_DATA,
              &rl); /* set maximum size of process's data segment */

    redirstdio(argc, argv); /*redirect slave's output*/

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

    if (jia_pid != 0) { // slave does
        VERBOSE_LOG(3, "I am %d, running here\n", jia_pid);
    }
    enable_sigio();

    timel = jia_current_time();
    time1 = jia_clock();

    if (jia_pid == 0)
        VERBOSE_LOG(3, "End of Initialization\n");
}

#ifdef DOSTAT
void clearstat() // initialized jiastat with 0
{
    memset((char *)&jiastat, 0, sizeof(jiastat));
}
#endif

#else /* NULL_LIB */

#include <stdio.h>

int jia_pid = 0;
int hostc = 1;

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
