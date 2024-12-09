#ifndef UTILS_H
#define UTILS_H

#include "init.h"
#include "thread.h"
#include <pthread.h>
#include <stdio.h>

static int verbose_log = 3;
static int verbose_out = 3;
extern FILE *logfile;
static char *clientstr = "[Thread client]";
static char *serverstr = "[Thread server]";
static char *listenstr = "[Thread listen]";
static char *mainstr = "[Thread  main ]";

#define LOCKFREE

#define VERBOSE_LOG(level, fmt, ...)                                           \
    if (verbose_log >= level) {                                                \
        if (logfile != NULL)                                                   \
            fprintf(logfile, fmt, ##__VA_ARGS__);                              \
        else                                                                   \
            fprintf(stdout, fmt, ##__VA_ARGS__);                               \
    }

#define VERBOSE_OUT(level, fmt, ...)                                           \
    if (verbose_out >= level) {                                                \
        printf(fmt, ##__VA_ARGS__);                                            \
    }

#define show_errno() (errno == 0 ? "None" : strerror(errno))

#define log_err(STR, ...)                                                      \
    do {                                                                       \
        char *str;                                                             \
        if (pthread_self() == client_tid) {                                    \
            str = clientstr;                                                   \
        } else if (pthread_self() == server_tid) {                             \
            str = serverstr;                                                   \
        } else if (pthread_self() == listen_tid) {                             \
            str = listenstr;                                                   \
        } else {                                                               \
            str = mainstr;                                                     \
        }                                                                      \
        fprintf(stderr,                                                        \
                "[\033[31mERROR\033[0m] %s (%s:%d:%s: errno: %s) " STR "\n",   \
                str, __FILE__, __LINE__, __func__, show_errno(),               \
                ##__VA_ARGS__);                                                \
    } while (0)

#define log_info(level, STR, ...)                                              \
    if (verbose_log >= level) {                                                \
        char *str;                                                             \
        if (pthread_self() == client_tid) {                                    \
            str = clientstr;                                                   \
        } else if (pthread_self() == server_tid) {                             \
            str = serverstr;                                                   \
        } else if (pthread_self() == listen_tid) {                             \
            str = listenstr;                                                   \
        } else {                                                               \
            str = mainstr;                                                     \
        }                                                                      \
        fprintf(logfile, "[TIME: %lu] [INFO] %s (%s:%d:%s) " STR "\n",         \
                jia_current_time(), str, __FILE__, __LINE__, __func__,         \
                ##__VA_ARGS__);                                                \
    }

#define log_out(level, STR, ...)                                               \
    if (verbose_out >= level) {                                                \
        fprintf(stdout, "[INFO] (%s:%d:%s) " STR "\n", __FILE__, __LINE__,     \
                __func__, ##__VA_ARGS__);                                      \
    }

#define cond_check(COND, STR, ...)                                             \
    if (!(COND)) {                                                             \
        log_err(STR, ##__VA_ARGS__);                                           \
        exit(EXIT_FAILURE);                                                    \
    }

extern int oldsigiomask;
#define BEGINCS                                                                \
    {                                                                          \
        sigset_t newmask, oldmask;                                             \
        sigemptyset(&newmask);                                                 \
        sigaddset(&newmask, SIGIO);                                            \
        sigprocmask(SIG_BLOCK, &newmask, &oldmask);                            \
        oldsigiomask = sigismember(&oldmask, SIGIO);                           \
        VERBOSE_LOG(3, "Enter CS\t");                                          \
    }
#define ENDCS                                                                  \
    {                                                                          \
        if (oldsigiomask == 0)                                                 \
            enable_sigio();                                                    \
        VERBOSE_LOG(3, "Exit CS\n");                                           \
    }


int open_logfile(char *filename, int argc, char **argv);
char* op2name(int op);

/**
 * @brief set_nonblocking - set socket to nonblocking mode
 *
 * @param sockfd
 */
static inline void set_nonblocking(int sockfd) {
    int flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
}

#endif
