#ifdef __linux__
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

/* see https://stackoverflow.com/questions/35596220 */
FILE *errorpf_outfp = NULL;
const char *errorpf_prefix = "(noname)";
int errorpf_escseq = 0;		/* none     */
int errorpf_verbose = 0;	/* disabled */

#define CP const char *const
       CP errorpf_escseqname  [] = { "none", "ansi",       "script",    0 };
static CP prefix_escseq_start [] = { "", "\033[38;5;244m", "@PREFIX{",  0 };
static CP prefix_escseq_stop  [] = { "", "\033[0m",        "}PREFIX@",  0 };
static CP error_escseq_start  [] = { "", "\033[31;1m",     "@ERROR{",   0 };
static CP error_escseq_stop   [] = { "", "\033[0m",        "}ERROR@",   0 };
static CP verbose_escseq_start[] = { "", "\033[32m",       "@VERBOSE{", 0 };
static CP verbose_escseq_stop [] = { "", "\033[0m",        "}VERBOSE@", 0 };

#define STRLEN(str)	((str) ? strlen((str)) : 0)
#define UNUSED(expr)	(void)(expr)

static void vpf(int errnum, const char *fmt, va_list ap)
{
    char *str = NULL;
    const char *priority;
    const char *escseq_start;
    const char *escseq_stop;

    /* Even when the value of errnum is 0, it is treated as an error. But
       strerror() will not be called and its return value is not used. */
    if (errnum >= 0) {
        priority = "ERROR: ";
        escseq_start = error_escseq_start[errorpf_escseq];
        escseq_stop = error_escseq_stop[errorpf_escseq];
    } else {
        priority = "";
        escseq_start = verbose_escseq_start[errorpf_escseq];
        escseq_stop = verbose_escseq_stop[errorpf_escseq];
    }

    if (!errorpf_outfp)
        return;

    if (fmt) {
        int ret = vasprintf(&str, fmt, ap);
        UNUSED(ret);
    }

    if (errnum <= 0) {
        if (STRLEN(str) <= 0) {
            /* If the errnum is less than or equal to 0, and the
               str is NULL or an empty string, nothing is output. */
        } else {
            fprintf(errorpf_outfp, "%s%s:%s %s%s%s%s\n",
                    prefix_escseq_start[errorpf_escseq],
                    errorpf_prefix,
                    prefix_escseq_stop[errorpf_escseq],
                    escseq_start,
                    priority,
                    str,
                    escseq_stop);
        }
    } else {
        if (STRLEN(str) <= 0) {
            fprintf(errorpf_outfp, "%s%s:%s %s%s%s%s\n",
                    prefix_escseq_start[errorpf_escseq],
                    errorpf_prefix,
                    prefix_escseq_stop[errorpf_escseq],
                    escseq_start,
                    priority,
                    strerror(errnum),
                    escseq_stop);
        } else {
            fprintf(errorpf_outfp, "%s%s:%s %s%s%s: %s%s\n",
                    prefix_escseq_start[errorpf_escseq],
                    errorpf_prefix,
                    prefix_escseq_stop[errorpf_escseq],
                    escseq_start,
                    priority,
                    str, strerror(errnum),
                    escseq_stop);
        }
    }

    if (str)
        free(str);

    fflush(errorpf_outfp);
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

void verbosepf(const char *fmt, ...)
{
    va_list ap;

    if (!errorpf_verbose)
        return;

    va_start(ap, fmt);
    vpf(-1, fmt, ap);
    va_end(ap);
}

/* vim: set et sw=4 sts=4: */
