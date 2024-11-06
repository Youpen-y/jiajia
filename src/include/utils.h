#ifndef UTILS_H
#define UTILS_H

#include "init.h"
#include <stdio.h>

static int verbose_log = 4;
static int verbose_out = 4;
extern FILE* logfile;

#define VERBOSE_LOG(level, fmt, ...)                                                               \
    if (verbose_log >= level) {                                                                        \
        fprintf(logfile, fmt, ##__VA_ARGS__);                                                                \
    }

#define VERBOSE_OUT(level, fmt, ...)                                                               \
    if (verbose_out >= level) {                                                                        \
        printf(fmt, ##__VA_ARGS__);                                                                \
    }

#define show_errno() (errno == 0 ? "None" : strerror(errno))

#define log_err(STR, ...)                                                      \
  fprintf(stderr, "[ERROR] (%s:%d:%s: errno: %s) " STR "\n", __FILE__,         \
          __LINE__, __func__, show_errno(), ##__VA_ARGS__)

#define log_info(level, STR, ...)                                              \
  if (verbose_log >= level) {                                                  \
    fprintf(logfile, "[INFO] (%s:%d:%s) " STR "\n", __FILE__, __LINE__,        \
            __func__, ##__VA_ARGS__);                                          \
  }

#define log_out(level, STR, ...)                                               \
  if (verbose_out >= level) {                                                  \
    fprintf(stdout, "[INFO] (%s:%d:%s) " STR "\n", __FILE__, __LINE__,         \
            __func__, ##__VA_ARGS__);                                          \
  }

#define cond_check(COND, STR, ...)                                             \
  if (!(COND)) {                                                               \
    log_err(STR, ##__VA_ARGS__);                                               \
    exit(EXIT_FAILURE);                                                        \
  }

/**
 * @brief open_logfile - open logfile
 * 
 * @param filename 
 * @return int 
 */
int open_logfile(char *filename);
#endif
