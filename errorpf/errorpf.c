#ifdef __linux__
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

FILE *pfout = NULL;
const char *pfprefix = "(noname)";
int pfescseq = 0;	/* none */
int pfverbose = 0;

#define CP const char *const
       CP pfescseqname          [] = { "none", "ansi",       "script",    0 };
static CP pfprefix_escseq_start [] = { "", "\033[38;5;244m", "@PREFIX{",  0 };
static CP pfprefix_escseq_stop  [] = { "", "\033[0m",        "}PREFIX@",  0 };
static CP errorpf_escseq_start  [] = { "", "\033[31;1m",     "@ERROR{",   0 };
static CP errorpf_escseq_stop   [] = { "", "\033[0m",        "}ERROR@",   0 };
static CP verbosepf_escseq_start[] = { "", "\033[32m",       "@VERBOSE{", 0 };
static CP verbosepf_escseq_stop [] = { "", "\033[0m",        "}VERBOSE@", 0 };

#define STRLEN(str)	((str) ? strlen((str)) : 0)
#define UNUSED(expr)	(void)(expr)

static void vpf(int errnum, const char *fmt, va_list ap)
{
    char *str = NULL;
    const char *priority;
    const char *escseq_start;
    const char *escseq_stop;

    /* errnum == 0 は (strerror(errnum) は使われないが) エラー扱いとする */
    if (errnum >= 0) {
        priority = "ERROR: ";
        escseq_start = errorpf_escseq_start[pfescseq];
        escseq_stop = errorpf_escseq_stop[pfescseq];
    } else {
        priority = "";
        escseq_start = verbosepf_escseq_start[pfescseq];
        escseq_stop = verbosepf_escseq_stop[pfescseq];
    }

    if (!pfout)
        return;

    if (fmt) {
        int ret = vasprintf(&str, fmt, ap);
        UNUSED(ret);
    }

    if (errnum <= 0) {
        if (STRLEN(str) <= 0) {
            /* errnum <= 0 で str が NULL or "" ならば何も出力されない */
        } else {
            fprintf(pfout, "%s%s:%s %s%s%s%s\n",
                    pfprefix_escseq_start[pfescseq],
                    pfprefix,
                    pfprefix_escseq_stop[pfescseq],
                    escseq_start,
                    priority,
                    str,
                    escseq_stop);
        }
    } else {
        if (STRLEN(str) <= 0) {
            fprintf(pfout, "%s%s:%s %s%s%s%s\n",
                    pfprefix_escseq_start[pfescseq],
                    pfprefix,
                    pfprefix_escseq_stop[pfescseq],
                    escseq_start,
                    priority,
                    strerror(errnum),
                    escseq_stop);
        } else {
            fprintf(pfout, "%s%s:%s %s%s%s: %s%s\n",
                    pfprefix_escseq_start[pfescseq],
                    pfprefix,
                    pfprefix_escseq_stop[pfescseq],
                    escseq_start,
                    priority,
                    str, strerror(errnum),
                    escseq_stop);
        }
    }

    if (str)
        free(str);

    fflush(pfout);
}

void exitpf(int exitcode, int errnum, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vpf(errnum, fmt, ap);
    va_end(ap);

    exit(exitcode);
}

void errorpf(int errnum, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vpf(errnum, fmt, ap);
    va_end(ap);
}

void verbosepf(int errnum, const char *fmt, ...)
{
    va_list ap;

    if (!pfverbose)
        return;

    va_start(ap, fmt);
    vpf(errnum, fmt, ap);
    va_end(ap);
}

/* vim: set et sw=4 sts=4: */
