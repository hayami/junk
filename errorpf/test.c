#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "errorpf.h"

static void usage()
{
    fprintf(stderr,
    "usage: %s [OPTION]\n"
    "  -c <color>    Display error/verbose message in color. Specify either\n"
    "                'ansi', 'none', or 'script' for the <color> argument.\n"
    "  -v            Verbose mode; Print diagnostic messages.\n", pfprefix);
}

int main(int argc, char *argv[])
{
    extern int optind;
    extern char *optarg;
    int i;
    char c;
    const char *fmt = "Hello World #%d";

    pfout = stderr;
    pfprefix = "test";

    while ((c = getopt(argc, argv, "c:v")) != -1) {
        switch (c) {
        case 'c':
            for (i = 0; ; i++) {
                const char *str = pfescseqname[i];
                if (str == NULL) {
                    usage();
                    return 1;
                }
                if (strcmp(optarg, str) == 0)
                    break;
            }
            pfescseq = i;
            break;
        case 'v':
            pfverbose = 1;
            break;
        case '?':
        default:
            usage();
            return 1;
        }
    }

    if (optind != argc) {
        usage();
        return 1;
    }

    errorpf(0, NULL);
    errorpf(0, fmt, 1);
    errorpf(1, NULL);
    errorpf(2, fmt, 2);

    verbosepf(NULL);
    verbosepf(fmt, 3);

    exitpf(0, 5, fmt, 4);

    return 1;
}
