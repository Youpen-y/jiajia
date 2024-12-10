#include "utils.h"
#include "init.h"
#include "msg.h"
#include "setting.h"
#include <libgen.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

// void init_msg(jia_msg_t* msg, int from, int to, int op){
//     msg->frompid = from;
//     msg->topid = to;
//     msg->op = op;
//     log_info(3, "msg", ...)
// }