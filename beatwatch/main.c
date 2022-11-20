#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "errorpf.h"
#include "global.h"

int debug = 0;
int last_exit_code = -1;

#define initial_ctrl_log_filename   NULL
#define default_ctrl_log_filename   "ctrl.log"
static const char *ctrl_log_filename = initial_ctrl_log_filename;

#define initial_ctrl_kfd            (-1)
#define default_ctrl_kfd            (3)
int ctrl_kfd = initial_ctrl_kfd;

#define initial_onexit_script      NULL
static const char *onexit_script = initial_onexit_script;

static int parsearg(int argc, char *argv[]);
static const char *getnextarg(int *indexp, int argc, char *argv[]);
static const char *optargmatch(const char *opt, const char *arg);
static int boolstr2int(const char *str);
static void usage(int status);

int main(int argc, char *argv[]) {
    int ret;

    errorpf_outfp = stderr;
    errorpf_prefix = "beatwatch (command-line)";

    exec_argv = argv + parsearg(argc, argv);

    init_sighandler();

    ret = cmdline();

    last_exit_code = ret;
    return ret;
}

static int parsearg(int argc, char *argv[]) {
    int ret, index;
    const char *cmdarg = NULL;

    index = 1;
    for (const char *arg1 = NULL, *arg2 = NULL; ; arg1 = arg2, arg2 = NULL) {
        const char *str;

        if (arg1 == NULL) {
            arg1 = getnextarg(&index, argc, argv);
            if (arg1 == NULL)
                usage(OTHER_ERROR_EXIT);
        }
        arg2 = getnextarg(&index, argc, argv);

        /* parse: -- (argument separator) */
        str = optargmatch("--", arg1);
        if (str) {
            arg1 = NULL;
            if (*str == '\0' && arg2) {
                cmdarg = arg2;
                break;
            }
            usage(OTHER_ERROR_EXIT);
        }

        /* parse: --ctrl-fd <N> (or --control-fd <N>) */
        str = optargmatch("--ctrl-fd", arg1);
        if (!str)
            str = optargmatch("--control-fd", arg1);
        if (str) {
            arg1 = NULL;
            ctrl_log_filename = NULL;
            if (*str == '\0') {
                if (arg2 && strncmp("--", arg2, 2) != 0) {
                    str = arg2;
                    arg2 = NULL;
                } else {
                    ctrl_kfd = default_ctrl_kfd;
                    str = NULL;
                }
            } else {
                /* *str == '=' */
                str++;
            }
            if (str) {
                if (strlen(str) > 0) {
                    char *end = NULL;
                    if ((ctrl_kfd = strtol(str, &end, 10)) >= 3
                        && *end == '\0') {
                    } else {
                        usage(OTHER_ERROR_EXIT);
                    }
                } else {
                    /* reset to initial state */
                    ctrl_kfd = initial_ctrl_kfd;
                }
            }
#ifdef DEBUG_ARG_PARSER
            printf("--ctrl-fd=\"%d\"\n", ctrl_kfd);
#endif
            continue;
        }

        /* parse: --ctrl-log <F> (or --control-log <F>) */
        str = optargmatch("--ctrl-log", arg1);
        if (!str)
            str = optargmatch("--control-log", arg1);
        if (str) {
            arg1 = NULL;
            ctrl_log_filename = NULL;
            if (*str == '\0') {
                if (arg2 && strncmp("--", arg2, 2) != 0) {
                    str = arg2;
                    arg2 = NULL;
                } else {
                    ctrl_log_filename = default_ctrl_log_filename;
                    str = NULL;
                }
            } else {
                /* *str == '=' */
                str++;
            }
            if (str) {
                if (strlen(str) > 0) {
                    ctrl_log_filename = str;
                } else {
                    /* reset to initial state */
                    ctrl_log_filename = initial_ctrl_log_filename;
                }
            }
#ifdef DEBUG_ARG_PARSER
            printf("--ctrl-log=\"%s\"\n", ctrl_log_filename);
#endif
            continue;
        }

        /* parse and set: --debug */
        str = optargmatch("--debug", arg1);
        if (str) {
            arg1 = NULL;
            if (*str == '\0') {
                debug = 1;
            }
            else if (*str++ == '=') {
                if (strlen(str) > 0) {
                    ret = boolstr2int(str);
                    if (ret < 0)
                        usage(OTHER_ERROR_EXIT);
                    debug = ret;
                } else {
                    /* reset to initial state */
                    debug = 0;
                }
            }
#ifdef DEBUG_ARG_PARSER
            printf("--debug=\"%d\"\n", debug);
#endif
            continue;
        }

        /* parse: --on-exit-script <S> */
        str = optargmatch("--on-exit-script", arg1);
        if (!str)
            str = optargmatch("--on-exit", arg1);
        if (str) {
            arg1 = NULL;
            onexit_script = NULL;
            if (*str == '\0') {
                if (arg2 && strncmp("--", arg2, 2) != 0) {
                    str = arg2;
                    arg2 = NULL;
                } else {
                    usage(OTHER_ERROR_EXIT);
                }
            } else {
                /* *str == '=' */
                str++;
            }
            if (str) {
                if (strlen(str) > 0) {
                    onexit_script = str;
                } else {
                    /* reset to initial state */
                    onexit_script = initial_onexit_script;
                }
            }
#ifdef DEBUG_ARG_PARSER
            printf("--on-exit-script=\"%s\"\n", onexit_script);
#endif
            continue;
        }

        /* parse and show: --help (or --usage) */
        str = optargmatch("--help", arg1);
        if (!str)
            str = optargmatch("--usage", arg1);
        if (str) {
            arg1 = NULL;
            if (*str == '\0')
                usage(NORMAL_EXIT);
            usage(OTHER_ERROR_EXIT);
        }

        if (arg1) {
            /* not match above */
            cmdarg = arg1;
            break;
        }
    }

    /* remaining arguments are passed to exec_argv */
    int next_arg_index = 0;
    for (int k = 1; k < argc; k++)
        if (cmdarg == argv[k]) {
            next_arg_index = k;
            break;
        }
    if (next_arg_index < 1 || argc <= next_arg_index)
        fixme(0);
#ifdef DEBUG_ARG_PARSER
    for (int i = next_arg_index; i < argc; i++) {
        printf("argv[%d]=\"%s\"\n", i, argv[i]);
    }
    exit(NORMAL_EXIT);
#endif

    /* set: --ctrl-log <F> (or --control-log <F>) */
    if (ctrl_log_filename) {
        /* Do fopen() in advance, and save fp for later use. If an error
           causes the program to stop, now is preferable because user who
           invoked this program is more likely to see the error message.
           In this case, doing fopen() later when needed is not good. */
        ctrl_logfp = fopen(ctrl_log_filename, "a");
        if (ctrl_logfp == NULL) {
            errorpf(errno, "fopen(%s)", ctrl_log_filename);
            exit(OTHER_ERROR_EXIT);
        }
    }

    /* set: --ctrl-fd <N> (or --control-fd <N>) */
    if (ctrl_kfd < 0)  {
        /* command line option is not specified for ctrl_kfd */
        ctrl_kfd = default_ctrl_kfd;

        /* Try to get the number 3 (default_ctrl_kfd) file descriptor.
           This should work if the samallest unopend file descriptor
           is 3. File descriptors 0, 1 and 2 should be opend as stdin,
           stdout and stderr respectively. */
        ret = open("/dev/null", O_WRONLY);
        if (ret < 0) {
            errorpf(errno, "open(/dev/null)");
            exit(FATAL_EXIT);
        }
        if (ret != ctrl_kfd) {
            errorpf(0, "failed to get file descriptor %d", ctrl_kfd);
            exit(OTHER_ERROR_EXIT);
        }
        /* The file descriptor ctrl_kfd mus be kept untouched
           until dup2() just before execvp(). */
        ctrl_wfd = -1;
    }
    else if (ctrl_kfd >= 3) {
        /* command line option is specified for ctrl_kfd */

        ret = fcntl(ctrl_kfd, F_GETFL);
        if (ret < 0) {
            errorpf(0, "fd %d is not open", ctrl_kfd);
            exit(OTHER_ERROR_EXIT);
        }
        if (!((ret & O_ACCMODE) == O_WRONLY ||
              (ret & O_ACCMODE) == O_RDWR)) {
            errorpf(0, "fd %d is not writable", ctrl_kfd);
            exit(OTHER_ERROR_EXIT);
        }
        /* The file descriptor ctrl_kfd mus be kept untouched
           until dup2() just before execvp(). */
        ret = dup(ctrl_kfd);
        if (ret < 0) {
            errorpf(errno, "dup()");
            exit(FATAL_EXIT);
        }
        ctrl_wfd = ret;
    }
    else {
        /* Do not use 0 (stdin), 1 (stdout), 2 (stderr) for ctrl_kfd */
        fixme(0);
    }

    /* set: --on-exit-script <S> */
    if (onexit_script) {
        ret = set_onexit_script(onexit_script);
        if (ret < 0) {
            errorpf(errno, "malloc()");
            exit(FATAL_EXIT);
        }
    }

    return next_arg_index;
}

static const char *getnextarg(int *indexp, int argc, char *argv[]) {
    const char *ret;

    do {
        if (*indexp >= argc) {
            return NULL;
        }
        ret = argv[*indexp];
        (*indexp)++;
    } while (!ret);

    return ret;
}

static const char *optargmatch(const char *opt, const char *arg) {

    /* leading double hyphen is always requierd */
    if (!(*opt++ == '-' && *arg++ == '-')) {
        return NULL;
    }
    if (!(*opt++ == '-' && *arg++ == '-')) {
        return NULL;
    }

    while (!(*opt == '\0' || *arg == '\0')) {
        if (!(*opt++ == *arg++)) {
            return NULL;
        }

        /* ignore one hyphen differences */
        if (*opt == '-') {
            opt++;
        }
        if (*arg == '-') {
            arg++;
        }
    }

    if (*opt == '\0') {
        if  (*arg == '=' || *arg == '\0')
            return arg;
    }
    return NULL;
}

static int boolstr2int(const char *str) {
    struct {
        const char *str;
        int ret;
    } dic[] = {
        { "true",    1 }, { "false",   0 },
        { "1",       1 }, { "0",       0 },
        { "enable",  1 }, { "disable", 0 },
        { "on",      1 }, { "off",     0 },
        { "yes",     1 }, { "no",      0 },
        { NULL,     -1 }
    };

    for (int i = 0; dic[i].str; i++) {
        if (strcasecmp(dic[i].str, str) == 0)
            return dic[i].ret;
    }
    return -1;
}

static void usage(int status) {
    fprintf(stderr,
    "usage: beatwatch [OPTION]... [--] COMMAND [ARG]...\n"
    "options:\n"
    "  --ctrl-fd <N>    If this option is used, <N> is used for control file\n"
    "                   descriptor number. Using this option without <N>, or\n"
    "                   this option is not used at all, %d is used for it.\n"
    "\n"
    "  --ctrl-log <F>   If this option is used, control log is appended to\n"
    "                   a file named <F>. Using this option without <F>,\n"
    "                   \"%s\" is used as the file name.\n"
    "\n"
    "  --debug          Enabe debug mode.\n"
    "\n"
    "  --on-exit <S>    If this option is used, script <S> is executed by\n"
    "                   /bin/sh. At the time before the script is executed,\n"
    "                   the exit status of beatwatch (watchdog) is set to the\n"
    "                   $? shell variable.\n"
    "\n"
    "  --help, --usage  Display this help and exit.\n",
    default_ctrl_kfd, default_ctrl_log_filename);

    exit(status);
}

/* vim: set et sw=4 sts=4: */
