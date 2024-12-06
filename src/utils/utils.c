#include "utils.h"
#include "init.h"
#include "msg.h"
#include "setting.h"
#include <libgen.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

FILE *logfile = NULL;

int open_logfile(char *filename, int argc, char **argv) {
    char name[30];
    if (system_setting.jia_pid == 0) {
        logfile = fopen(filename, "w+");
    } else {
        sprintf(name, "./jianode/%s/%s", basename(argv[0]), filename);
        logfile = fopen(name, "w+");
    }
    if (logfile == NULL) {
        printf("Cannot open logfile %s\n", filename);
        return -1;
    }
    return 0;
}

char* op2name(int op){
    switch (op) {
    case DIFF:
        return "DIFF";
    case DIFFGRANT:
        return "DIFFGRANT";
    case GETP:
        return "GETP";
    case GETPGRANT:
        return "GETPGRANT";
    case ACQ:
        return "ACQ";
    case ACQGRANT:
        return "ACQGRANT";
    case INVLD:
        return "INVALID";
    case BARR:
        return "BARR";
    case BARRGRANT:
        return "BARRGRANT";
    case REL:
        return "REL";
    case WTNT:
        return "WTNT";
    case JIAEXIT:
        return "JIAEXIT";
    case WAIT:
        return "WAIT";
    case WAITGRANT:
        return "WAITGRANT";
    case STAT:
        return "STAT";
    case STATGRANT:
        return "STATGRANT";
    case SETCV:
        return "SETCV";
    case RESETCV:
        return "RESETCV";
    case WAITCV:
        return "WAITCV";
    case CVGRANT:
        return "CVGRANT";
    case MSGBODY:
    case MSGTAIL:
        return "MSG";
    case LOADREQ:
        return "LOADREQ";
    case LOADGRANT:
        return "LOADGRANT";

    default:
        return "NULL";
    }
}

// void init_msg(jia_msg_t* msg, int from, int to, int op){
//     msg->frompid = from;
//     msg->topid = to;
//     msg->op = op;
//     log_info(3, "msg", ...)
// }