#include "utils.h"
#include "init.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

extern FILE* config;


/**
 * @brief my_getline -- get
 *
 * @param wordc
 * @param wordv
 * @return int
 */
int my_getline(int *wordc, char wordv[Maxwords][Wordsize]) {
    char line[Linesize];
    char ch;
    int linei, wordi1, wordi2;
    int note = 0;

    //  reading a line in the configuration file except the comment
    linei = 0;
    while (((ch = getc(config)) != '\n') && (ch != EOF)) {
        if (ch == '#')
            note = 1;
        if (linei < Linesize - 1 && !note) {
            line[linei] = ch;
            linei++;
        }
    }
    line[linei] = '\0';

    // for (wordi1=0;wordi1<Maxwords;wordi1++)
    //   wordv[wordi1][0]='\0';
    
    // Parse strings to tokens
    wordi1 = 0;
    linei = 0;
    while ((line[linei] != '\0') && (wordi1 < Maxwords)) {
        while ((line[linei] == ' ') || (line[linei] == '\t'))
            linei++;
        wordi2 = 0;
        while ((line[linei] != ' ') && (line[linei] != '\t') &&
               (line[linei] != '\0')) {
            if (wordi2 < Wordsize - 1) {
                wordv[wordi1][wordi2] = line[linei];
                wordi2++;
            }
            linei++;
        }
        if (wordi2 > 0) {
            wordv[wordi1][wordi2] = '\0';
            wordi1++;
        }
    }

    *wordc = wordi1;
    return (ch == EOF);
}