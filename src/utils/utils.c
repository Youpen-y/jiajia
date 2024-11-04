#include "utils.h"
#include "init.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

FILE *logfile;

int open_logfile(char *filename) {
    logfile = fopen(filename, "w");
    if (logfile == NULL) {
        printf("Cannot open logfile %s\n", filename);
        return -1;
    }
    return 0;
}