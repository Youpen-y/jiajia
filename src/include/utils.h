#ifndef UTILS_H
#define UTILS_H

#include "init.h"
#include <stdio.h>

static int verbose_log = 3;
static int verbose_out = 3;
extern FILE *logfile;
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
        \ fprintf(stderr,                                                      \
                  "[\033[31mERROR\033[0m] (%s:%d:%s: errno: %s) " STR "\n",    \
                  __FILE__, __LINE__, __func__, show_errno(), ##__VA_ARGS__);  \
    } while (0)

#define log_info(level, STR, ...)                                              \
    if (verbose_log >= level) {                                                \
        \ fprintf(logfile, "[TIME: %lu] [INFO] (%s:%d:%s) " STR "\n",          \
                  jia_current_time(), __FILE__, __LINE__, __func__,            \
                  ##__VA_ARGS__);                                              \
    }

#define log_out(level, STR, ...)                                               \
    if (verbose_out >= level) {                                                \
        fprintf(stdout, "[INFO] (%s:%d:%s) " STR "\n", __FILE__, __LINE__,     \
                __func__, ##__VA_ARGS__);                                      \
    }

int my_getline(int *wordc, char wordv[Maxwords][Wordsize]);

#endif