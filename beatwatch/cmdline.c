#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "errorpf.h"
#include "global.h"

static const char done = '%';   /* done mark */
static int child_posttask(void);
static int parent_posttask(int child_pid);
static int ctrlmsg_handler(char, int, const char *, int *);

int cmdline() {
    int ret;
    struct {
        /* exit code got from below */
        int ctrlmsg;
        int sigcause;
    } exitcode = { -1, -1 };

    int child_pid = pipe_and_fork();

    if (child_pid == 0) {
        /* child side */
        ret = child_posttask();
        exit(ret);
    }

    /* parent side */
    int watchdog_pid = parent_posttask(child_pid);

    if (watchdog_pid == 0) {
        /* got an EOF */
        noticepf("watchdog has disconnected control channel");
        goto wayout;
    }

    /* cmdline main loop */
    char type;
    int val;
    const char *line;
    while (1) {
        ret = recv_ctrlmsg(&type, &val, &line);

        if (ret == 0) {
            /* got an EOF */

            noticepf("watchdog has disconnected control channel");
            break;
        }
        else if (ret < 0) {
            /* interrupted */

            ret = continue_sighandler();
            if (ret >= 0) {
                exitcode.sigcause = ret;
                break;
            }
            continue;
        }
        else if (ret > 0) {
            /* got a control message */

            ret = ctrlmsg_handler(type, val, line, &exitcode.ctrlmsg);
            if (ret > 0)
                break;
        }
    }

wayout:
    /* close ctrl_rfd, but ctrl_wfd is still needed in onexit() */
    if (ctrl_rfd >= 0) {
        close(ctrl_rfd);
        ctrl_rfd = -1;
    }

    ret = NORMAL_EXIT;
    ret = (exitcode.ctrlmsg  < 0) ? ret : exitcode.ctrlmsg;
    ret = (exitcode.sigcause < 0) ? ret : exitcode.sigcause;

    if (exitcode.ctrlmsg >= 0 && exitcode.sigcause < 0) {
        /* Do not send EXIT=N control message again. It has alread been sent. */
        if (ctrl_wfd >= 0) {
            close(ctrl_wfd);
            ctrl_wfd = -1;
        }
    }

    return ret;
}

static int child_posttask() {
    /* detach from the controlling terminal. watchdog becomes a new
       session leader, and also becomes a new process group leader. */
    errorpf_prefix = "beatwatch (transient-watchdog)";
    static const int nochdir = 1;
    static const int noclose = 1;
    int ret = daemon(nochdir, noclose);
    if (ret != 0) {
        errorpf(errno, "daemon()");
        exit(FATAL_EXIT);
    }
    errorpf_prefix = "beatwatch (watchdog)";
    /* At this point, the parent side can go beyond the waitpid() */

    /* child created via fork() inherits a copy of its parent's signal
       dispositions, but not inherit its parent's interval timer. */
    set_alarm_timer();

    send_ctrlmsgf("KILLPID=%d", getpid());

    return watchdog();
}

static int parent_posttask(int child_pid) {
    int ret;

    set_killpid(child_pid);

    /* The ctrl_kfd must be kept until dup2() just before execvp(),
       but is not needed from this point. So, close it here. */
    if (ctrl_kfd >= 0) {
        close(ctrl_kfd);
        ctrl_kfd = -1;
    }

    /* only watchdog can run run_onexit_script() */
    set_onexit_script(NULL);

    /* only watchdog can write to ctrl_logfp */
    if (ctrl_logfp != NULL) {
        fclose(ctrl_logfp);
        ctrl_logfp = NULL;
    }

    /* Wait for daemon() on the child side to complete */
    int status;
    do {
        status = 0;
        ret = waitpid(child_pid, &status, 0);
        if (ret < 0) {
            errorpf(errno, "waitpid()");
            exit(FATAL_EXIT);
        }
        if (WIFSIGNALED(status)) {
            errorpf(0, "transient-watchdog got a signal %d", WTERMSIG(status));
            exit(SIGNAL_EXIT(WTERMSIG(status)));
        }
    } while (!WIFEXITED(status));

    if (WEXITSTATUS(status) != 0) {
        errorpf(0, "transient-watchdog exited with non-zero exit code %d", WEXITSTATUS(status));
        exit(FATAL_EXIT);
    }

    char type;
    int pid;
    const char *line;
    while (1) {
        ret = recv_ctrlmsg(&type, &pid, &line);
        if (ret < 0) {
            /* interrupted */
            continue;
        }
        if (ret == 0) {
            /* got an EOF */
            return 0;
        }
        break;
    }
    if (type != 'K' || pid <= 0) {
        errorpf(0, "unexpected control message received");
        /* child process should be terminating */
        return 0;
    }

    /* need update pid */
    set_killpid(pid);

    send_ctrlmsgf("%c %s", done, line);

    return pid;
}

static int ctrlmsg_handler(char type, int val, const char *line, int *rexit) {
    int ret;

    switch (type) {

        case 'B':   /* BYE          */
        case 'D':   /* DETACH       */
            set_killpid(-1);
            if (ctrl_rfd >= 0) {
                close(ctrl_rfd);
                ctrl_rfd = -1;
            }
            send_ctrlmsgf("%c %s", done, line);
            ret = 1;
            break;

        case 'E':   /* EXIT=N       */
            *rexit = val;
            send_ctrlmsgf("%c %s", done, line);
            ret = 0;
            break;

        case 'T':   /* TIMEOUT=N    */
            set_timeout(val);
            send_ctrlmsgf("%c %s", done, line);
            ret = 0;
            break;

        case 'K':   /* KILLPID=N    */
        default:
            send_ctrlmsgf("%s", line);
            ret = 0;
            break;
    }

    return ret;
}

/* vim: set et sw=4 sts=4: */
