#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/time.h>

#include "errorpf.h"
#include "global.h"

#define SIGTERM_TIMEOUT     (0)
#define SIGKILL_TIMEOUT     (0 - (SIGKILL_DELAY * (1000 / UNIT_TIME)))
#define TIMER_FINISHED      (SIGKILL_TIMEOUT - 1)
#define TIMER_INITIAL       (SIGKILL_TIMEOUT - 2)

static int killpid = -1;
static int timer = TIMER_INITIAL;
static int received_abort_signo = 0;
static int received_alarm_signo = 0;
static char *onexit_script_with_prefix = NULL;

static void set_abort_handler(int signo);
static void set_alarm_handler(void);
static void asyncsafe_sighandler(int signo);
static int killping(void);
static int killsig(int signo);
static void onexit(void);
static void run_finalkiller(void);
static void run_onexit_script(void);
static void check_fdleak(void);

void set_killpid(int pid) {
    if (timer == TIMER_INITIAL) {
        /* skip the check below */
    } else {
        if (timer <= SIGTERM_TIMEOUT) {
            /* It's too late */
            return;
        }
    }
    killpid = pid;

    if (timer < BASE_TIMEOUT)
        set_timeout(BASE_TIMEOUT);
}

void set_timeout(int timeout) {
    if (timer == TIMER_INITIAL) {
        /* skip the check below */
    } else {
        if (timer <= SIGTERM_TIMEOUT) {
            /* no turning back now */
            return;
        }
    }

    /* The timeout is set after the seconds specified by
       the 'timeout' argument, but the actual timeout may
       be up to UNIT_TIME millisecond earlier, due to the
       design of this code. */
    timer = timeout * (1000 / UNIT_TIME);
}

void init_sighandler() {
    /* As described in the signal(7), The default action for SIGPIPE
       is "Term". So, if SIGPIPE is not caught, blocked, or ignored,
       the process will be terminated when SIGPIPE is received. This
       is explained in the write(2). */
    signal(SIGPIPE, SIG_IGN);

    set_abort_handler(SIGHUP);
    set_abort_handler(SIGINT);
    set_abort_handler(SIGQUIT);
    set_abort_handler(SIGTERM);

    set_alarm_handler();

    int ret = atexit(onexit);
    if (ret != 0) {
        errorpf(errno, "atexit()");
        exit(FATAL_EXIT);
    }
}

static void set_abort_handler(int signo) {
    struct sigaction act;

    memset(&act, 0, sizeof(act));
    act.sa_handler = asyncsafe_sighandler;
    act.sa_flags = 0;
    sigemptyset(&act.sa_mask);
    int ret = sigaction(signo, &act, NULL);
    if (ret != 0) {
        errorpf(errno, "sigaction(%d)", signo);
        exit(FATAL_EXIT);
    }
}

static void set_alarm_handler() {
    struct sigaction act;

    memset(&act, 0, sizeof(act));
    act.sa_handler = asyncsafe_sighandler;
    act.sa_flags = 0;
    sigemptyset(&act.sa_mask);
    int ret = sigaction(SIGALRM, &act, NULL);
    if (ret != 0) {
        errorpf(errno, "sigaction(SIGALRM)");
        exit(FATAL_EXIT);
    }

    set_alarm_timer();
}

/* child created via fork() inherits a copy of its parent's signal
   dispositions, but not inherit its parent's interval timer. */
void set_alarm_timer() {
    struct itimerval it;

    /* set UNIT_TIME millisecond interval timer */
    memset(&it, 0, sizeof(it));
    it.it_interval.tv_sec = UNIT_TIME / 1000;
    it.it_interval.tv_usec = (UNIT_TIME % 1000) * 1000;
    it.it_value.tv_sec = UNIT_TIME / 1000;
    it.it_value.tv_usec = (UNIT_TIME % 1000) * 1000;
    int ret = setitimer(ITIMER_REAL, &it, NULL);
    if (ret != 0) {
        errorpf(errno, "setitimer()");
        exit(FATAL_EXIT);
    }
}

static void asyncsafe_sighandler(int signo) {
    if (signo == SIGALRM)
        received_alarm_signo = signo;
    else
        received_abort_signo = signo;
}

static int killping() {
    return killsig(0);
}

static int killsig(int signo) {
    int ret;

    if (killpid == -1 ||
       ((ret = kill(killpid, signo)) == -1 && errno == ESRCH)) {

        /* The process of killpid PID is already dead */
        killpid = -1;
        return 1;
    }
    /* If ret is 0, the process is still alive.
       If ret is -1, errno is EINVAL or EPERM. */
    return ret;
}

int continue_sighandler() {
    static int expected = -1;   /* expected exit code */

    if (received_abort_signo) {

        if (timer <= SIGTERM_TIMEOUT) {
            /* It's too late */
            received_abort_signo = 0;
        } else {
            if (expected < 0)
                expected = SIGNAL_EXIT(received_abort_signo);

            errorpf(-1, "received signal #%d, terminating", received_abort_signo);
            received_abort_signo = 0;

            timer = SIGTERM_TIMEOUT;
            goto breakin;
        }
    }

    if (received_alarm_signo) {
        received_alarm_signo = 0;

        if (timer <= TIMER_FINISHED)
            goto normal_return;

        timer--;
        if (timer < SIGKILL_TIMEOUT || SIGTERM_TIMEOUT < timer)
            goto normal_return;

breakin:
        if (killping() > 0)
            goto vanished;

        if (timer == SIGTERM_TIMEOUT) {
            if (expected < 0) {
                expected = TIMEOUT_EXIT;
                errorpf(-1, "timed out, PID=%d will now be terminated", killpid);
            }
            noticepf("sending SIGTERM to PID=%d", killpid);
            if (killsig(SIGTERM) > 0)
                goto vanished;
        }
        if (timer == SIGKILL_TIMEOUT) {
            noticepf("sending SIGKILL to PID=%d", killpid);
            if (killsig(SIGKILL) > 0)
                goto vanished;

            /* assume the process is dead */
            killpid = -1;
            goto vanished;
        }
    }

normal_return:
    return -1;

vanished:
    timer = TIMER_FINISHED;
    return expected;
}

static void onexit() {
    timer = TIMER_FINISHED;

    if (last_exit_code < 0) {
        /* last_exit_code is not set. The exit(3) is called directly
           instead of using exit() defined with macro in lastexit.h */
        fixme(0);
    }

    run_finalkiller();

    if (ctrl_wfd >= 0) {
        send_ctrlmsgf_without_error_handling("EXIT=%d", last_exit_code);
        close(ctrl_wfd);
        ctrl_wfd = -1;
    }

    run_onexit_script();

    if (ctrl_logfp) {
        fclose(ctrl_logfp);
        ctrl_logfp = NULL;
    }

    if (debug)
        check_fdleak();
}

static void run_finalkiller() {
    int ret, count, interval = 100;  /* millisecond */

    /* Here, it is assumed that program execution will
       go here from situations where it is difficult to
       continue, such as when exit(FATAL_EXIT) is called. */
    if (killpid == -1)
        return;

    ret = killping();
    if (ret > 0)
        return;

    count = HOLDON_DELAY * (1000 / interval);
    while (count-- > 0) {
        usleep(interval * 1000);

        ret = killping();
        if (ret > 0)
            return;
    }

    noticepf("sending SIGTERM to PID=%d", killpid);
    ret = killsig(SIGTERM);
    if (ret > 0)
        return;

    count = SIGKILL_DELAY * (1000 / interval);
    while (count-- > 0) {
        usleep(interval * 1000);

        ret = killping();
        if (ret > 0)
            return;
    }

    noticepf("sending SIGKILL to PID=%d", killpid);
    killsig(SIGKILL);
}

static void run_onexit_script() {
    if (onexit_script_with_prefix == NULL)
        return;

    char *cp = strstr(onexit_script_with_prefix, "%%%");
    if (cp == NULL)
        return;

    int q, code = last_exit_code & 0xff;
    q = code / 100; *cp++ = (q == 0) ? ' ' : '0' + q; code = code % 100;
    q = code /  10; *cp++ = (q == 0) ? ' ' : '0' + q; code = code %  10;
    q = code      ; *cp   = '0' + q;

    int ret = system(onexit_script_with_prefix);
    free(onexit_script_with_prefix);
    UNUSED(ret);
}

int set_onexit_script(const char *str) {
    if (onexit_script_with_prefix) {
        free(onexit_script_with_prefix);
        onexit_script_with_prefix = NULL;
    }

    if (str == NULL)
        return 0;

    int len = strlen(str);
    if (len <= 0)
        return 0;

#define SET_RETCODE "(return %%%); "    /* script to set $? in /bin/sh */

    onexit_script_with_prefix = malloc(strlen(SET_RETCODE) + len + 1);
    if (onexit_script_with_prefix == NULL)
        return -1;

    strcpy(onexit_script_with_prefix, SET_RETCODE);
    strcat(onexit_script_with_prefix, str);
    return 0;
}

static void check_fdleak() {
    for (int fd = 0; fd < 64; fd++) {
        int ret = fcntl(fd, F_GETFD);

        if (fd <= 2) {
            if (ret == -1) {
                errorpf(errno, "fd=%d is expected to be opened", fd);
            }
        } else {
            if (ret == -1) {
                if (errno == EBADF) {
                    /* OK: already closed */
                } else {
                    errorpf(errno, "unexpected error on fd=%d", fd);
                }
            } else {
               errorpf(0, "FD LEAK: fd=%d is expected to be closed", fd);
            }
        }
    }
}

/* vim: set et sw=4 sts=4: */
