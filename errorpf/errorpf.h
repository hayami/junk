#include <stdio.h>
#include <errno.h>
extern int errno;

extern FILE *pfout;
extern const char *pfprefix;
extern int pfverbose;
extern int pfescseq;
extern const char *const pfescseqname[];

void exitpf(int exitcode, int errnum, const char *fmt, ...);
void errorpf(int errnum, const char *fmt, ...);
void verbosepf(int errnum, const char *fmt, ...);

/* vim: set et sw=4 sts=4: */
