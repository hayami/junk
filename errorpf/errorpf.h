#include <stdio.h>
#include <errno.h>

extern FILE *pfout;
extern const char *pfprefix;
extern int pfverbose;
extern int pfescseq;
extern const char *const pfescseqname[];
extern int *pfexit_code_storagep;

void exitpf(int exitcode, int errnum, const char *fmt, ...);
void errorpf(int errnum, const char *fmt, ...);
void verbosepf(const char *fmt, ...);

/* vim: set et sw=4 sts=4: */
