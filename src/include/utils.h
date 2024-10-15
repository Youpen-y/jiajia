#ifndef UTILS_H
#define UTILS_H

#include "init.h"
#include <stdio.h>

static int verbose_log = 1;
static int verbose_out = 3;
extern FILE* logfile;
#define VERBOSE_LOG(level, fmt, ...)                                                               \
    if (verbose_log >= level) {                                                                        \
        fprintf(logfile, fmt, ##__VA_ARGS__);                                                                \
    }

#define VERBOSE_OUT(level, fmt, ...)                                                               \
    if (verbose_out >= level) {                                                                        \
        printf(fmt, ##__VA_ARGS__);                                                                \
    }

int my_getline(int *wordc, char wordv[Maxwords][Wordsize]);

#endif