#include <stdio.h>

#ifndef CONFIG_UNIT_TIME
#define CONFIG_UNIT_TIME        250 /* in millisecond; must be a divisor of 1000 */
#endif

#ifndef CONFIG_BASE_TIMEOUT
#define CONFIG_BASE_TIMEOUT     5   /* in seond */
#endif

#ifndef CONFIG_EXTRA_TIMEOUT
#define CONFIG_EXTRA_TIMEOUT    5   /* in seond */
#endif

#ifndef CONFIG_SIGKILL_DELAY
#define CONFIG_SIGKILL_DELAY    3   /* in seond */
#endif

#ifndef CONFIG_HOLDON_DELAY
#define CONFIG_HOLDON_DELAY     2   /* in seond */
#endif

#define UNIT_TIME               (CONFIG_UNIT_TIME)
#define BASE_TIMEOUT            (CONFIG_BASE_TIMEOUT)
#define EXTRA_TIMEOUT           (CONFIG_EXTRA_TIMEOUT)
#define SIGKILL_DELAY           (CONFIG_SIGKILL_DELAY)
#define HOLDON_DELAY            (CONFIG_HOLDON_DELAY)

#define NORMAL_EXIT             (0)
#define OTHER_ERROR_EXIT        (1) /* Errors other than the following */
#define TIMEOUT_EXIT            (2)
#define FATAL_EXIT              (9)
#define SIGNAL_EXIT(signo)      (128 + (signo))

#define exit(status)            (exit)(last_exit_code = (status))

#define fixme(errno)            errorpf((errno), "FIXME %s:%d", __FILE__, __LINE__)

#define UNUSED(var)             (void)(var)

extern int debug;
extern int last_exit_code;
extern int ctrl_kfd;
extern char *const *exec_argv;

int pipe_and_fork(void);
int cmdline(void);
int watchdog(void);
int execfunc(void);

/* sigmisc.c */
void set_killpid(int pid);
void set_timeout(int timeout);
void init_sighandler(void);
void set_alarm_timer(void);
int continue_sighandler(void);
int set_onexit_script(const char *str);

/* ctrl.c */
extern int ctrl_rfd;
extern int ctrl_wfd;
extern FILE *ctrl_logfp;
int recv_ctrlmsg(char *rtype, int *rval, const char **rline);
void send_ctrlmsgf(const char *fmt, ...);
void send_ctrlmsgf_without_error_handling(const char *fmt, ...);

/* vim: set et sw=4 sts=4: */
