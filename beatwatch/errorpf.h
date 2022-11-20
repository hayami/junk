#include "errorpf/errorpf.h"

/* Use of exitpf() is prohibited to force the macro version of
   exit() defined in lastexit.h to be used through the program.
   The exipf() uses exit(3) internally. */
#define exitpf(...)             USE_OF_exitpf_IS_PROHIBITED
#define verbosepf(...)          USE_OF_verbosepf_IS_PROHIBITED
#define aserrorpf(...)          USE_OF_aserrorpf_IS_PROHIBITED
#define asverbosepf(...)        USE_OF_asverbosepf_IS_PROHIBITED

#define errorpf(eno, fmt, ...)  do {							\
                                    char *line = NULL;					\
                                    (aserrorpf)(&line, (eno), (fmt), ##__VA_ARGS__);	\
                                    if (line) {						\
                                        fprintf(errorpf_outfp, "%s\n", line);		\
                                        send_ctrlmsgf("STDERR: %s", line);		\
                                        free(line);					\
                                    }							\
                                } while (0)

#define noticepf(fmt, ...)      do {							\
                                    char *line = NULL;					\
                                    (aserrorpf)(&line, -1, (fmt), ##__VA_ARGS__);	\
                                    if (line) {						\
                                        send_ctrlmsgf("NOTICE: %s", line);		\
                                        free(line);					\
                                    }							\
                                } while (0)

/* vim: set et sw=4 sts=4: */
