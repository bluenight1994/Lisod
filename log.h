//
// Created by 甘宏 on 08/02/2017.
//


#ifndef _LOG_H_
#define _LOG_H_

#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>

void Log(FILE *fp, char *msg,...);
FILE *open_log(const char *path);

#endif