#include <stdio.h>
#include <errno.h>

extern FILE *errorpf_outfp;
extern const char *errorpf_prefix;
extern int errorpf_verbose;
extern int errorpf_escseq;
extern const char *const errorpf_escseqname[];

void exitpf(int exitcode, int errnum, const char *fmt, ...);
void errorpf(int errnum, const char *fmt, ...);
void verbosepf(const char *fmt, ...);
void aserrorpf(char **strp, int errnum, const char *fmt, ...);
void asverbosepf(char **strp, const char *fmt, ...);

/* vim: set et sw=4 sts=4: */
