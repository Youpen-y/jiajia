#ifndef TOOLS_H
#define TOOLS_H
#pragma once

#include "mem.h"
#include "syn.h"
#include <comm.h>


/* */
extern char errstr[Linesize];


/* Function Declaration */
void inittools();
void assert0(int, char *, ...);
void assert(int cond, char *format, ...);
void jiaexitserver(jia_msg_t *req);
jia_msg_t *newmsg();
void freemsg(jia_msg_t *msg);
void appendmsg(jia_msg_t *msg, unsigned char *str, int len);
void printmsg(jia_msg_t *msg, int right);
void printstack(int ptr);
unsigned long jia_current_time();
float jia_clock();
void jiasleep(unsigned long);
void disable_sigio();
void enable_sigio();
void freewtntspace(wtnt_t *ptr);
wtnt_t *newwtnt();
void newtwin(address_t *twin);
void freetwin(address_t *twin);
void emptyprintf();
unsigned int get_usecs();


/**
 * @brief free_msg_index -- find a free msg index in msgarray
 * 
 * @return int index of free msg
 */
int free_msg_index();

#endif  /* TOOLS_H */