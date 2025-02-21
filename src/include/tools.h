#ifndef TOOLS_H
#define TOOLS_H
#include "rdma.h"
#pragma once

#include "mem.h"
#include "syn.h"
#include <comm.h>
#include <infiniband/verbs.h>
#include <pthread.h>

typedef void (* void_func_handler)();

typedef struct optimization {
    char name[20];
    bool flag;
} optimization_t;

struct prefetech_opt_t {
    optimization_t base;
    int prefetch_pages;     // num of pages that will be prefetched

    /* when prefetching pages, need to checking whether the home of the next page
    is current host. */
    int max_checking_pages;  // limit the pages that will be checked
};


/* Function Declaration */
void inittools();
void local_assert(int, char *, ...);
void jia_assert(int cond, char *format, ...);
void jiaexitserver(jia_msg_t *req);
jia_msg_t *newmsg();
void freemsg(jia_msg_t *msg);
void appendmsg(jia_msg_t *msg, unsigned char *str, int len);
void printmsg(jia_msg_t *msg);
void printstack(int ptr);
unsigned long jia_current_time();
double jia_clock();
void jiasleep(unsigned long);
void freewtntspace(wtnt_t *ptr);
wtnt_t *newwtnt();
void newtwin(address_t *twin);
void freetwin(address_t *twin);
void emptyprintf();
unsigned int get_usecs();


// RDMA related functions
/**
 * @brief print_port_info -- print ib port information
 */
void print_port_info(struct ibv_port_attr *port_attr);


/**
 * @brief free_msg_index -- find a free msg index in msgarray
 * 
 * @return int index of free msg
 */
int free_msg_index();


/**
 * @brief free_system_resources -- free system resources
 * 
 */
void free_system_resources();

/************log out****************/

static int verbose_log = 3;
static int verbose_out = 3;
extern FILE *logfile;
extern pthread_t server_tid;
extern pthread_t client_tid;
extern pthread_t listen_tid;
extern char errstr[Linesize];
static char *clientstr = "[Thread client]";
static char *serverstr = "[Thread server]";
static char *listenstr = "[Thread listen]";
static char *mainstr = "[Thread  main ]";

// optimization techniques
extern struct prefetech_opt_t prefetch_optimization;

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

#endif  /* TOOLS_H */