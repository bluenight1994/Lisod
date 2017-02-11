//
// Created by 甘宏 on 08/02/2017.
//
#include "log.h"


FILE *open_log(const char *path) {

    FILE *logfile;

    logfile = fopen(path, "w");

    if (logfile == NULL) {
        fprintf(stdout, "There is an error happen when trying to open log file.\n");
        exit(EXIT_FAILURE);
    }

    return logfile;
}

void print_time(FILE *fp) {
    time_t t;
    struct tm *Tm;

    t = time(NULL);
    Tm = localtime(&t);

    fprintf(fp, "[%04d-%02d-%02d %02d:%02d:%02d] ",
            Tm->tm_year+1900,
            Tm->tm_mon+1,
            Tm->tm_mday,
            Tm->tm_hour,
            Tm->tm_min,
            Tm->tm_sec
    );
}

void Log(FILE *fp, char *msg, ...) {
    print_time(fp);
    va_list ap;
    va_start(ap, msg);
    vfprintf(fp, msg, ap);
}


