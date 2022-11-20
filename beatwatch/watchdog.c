#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "errorpf.h"
#include "global.h"

static const char done = '+';   /* done mark */
static int child_posttask(void);
static int parent_posttask(int child_pid);
static int ctrlmsg_handler(char, int, const char *, int *);
static int waitpid_for_a_while(int child);
static int waitpid_nohang(int child);

int watchdog() {
    int ret;
    struct {
        /* exit code got from below */
        int waitpid;
        int ctrlmsg;
        int sigcause;
    } exitcode = { -1, -1, -1 };

    int child_pid = pipe_and_fork();

    if (child_pid == 0) {
        /* child side */
        ret = child_posttask();
        exit(ret);
    }

    /* parent side */
    ret = parent_posttask(child_pid);

    if (ret == 0) {
        /* got an EOF */
        noticepf("execfunc has disconnected control channel");
        goto wayout;
    }

    /* watchdog main loop */
    char type;
    int val;
    const char *line;
    while (1) {
        ret = recv_ctrlmsg(&type, &val, &line);

        if (ret == 0) {
            /* got an EOF */

            noticepf("monitoring-target has disconnected control channel");
            break;
        }
        else if (ret < 0) {
            /* interrupted */

            ret = waitpid_nohang(child_pid);
            if (ret >= 0) {
                exitcode.waitpid = ret;
                break;
            }

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
    if (exitcode.waitpid < 0) {
        ret = waitpid_for_a_while(child_pid);
        if (ret >= 0)
            exitcode.waitpid = ret;
    }

    /* close ctrl_rfd, but ctrl_wfd is still needed in onexit() */
    if (ctrl_rfd >= 0) {
        close(ctrl_rfd);
        ctrl_rfd = -1;
    }

    ret = NORMAL_EXIT;
    ret = (exitcode.waitpid  < 0) ? ret : exitcode.waitpid;
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
    /* only watchdog can run run_onexit_script() */
    set_onexit_script(NULL);

    /* only watchdog can write to ctrl_logfp */
    if (ctrl_logfp != NULL) {
        fclose(ctrl_logfp);
        ctrl_logfp = NULL;
    }

    /* disable run_finalkiller() in onexit() */
    set_killpid(-1);

    /* set new process group. monitoring-target becomes new process group
       leader while still belonging to watchdog's sesseion group. */
    errorpf_prefix = "beatwatch (monitoring-target)";
    int ret = setpgid(0, 0);
    if (ret != 0) {
        /* send an EOF immediately to parent */
        close(ctrl_wfd);
        ctrl_wfd = -1;

        /* Due to ctrl_wfd has closed just before, the following error
           message is sent only to stderr and ctrl_logfp (if not NULL). */
        errorpf(errno, "setpgid()");
        exit(FATAL_EXIT);
    }

    send_ctrlmsgf("KILLPID=%d", getpid());

    return execfunc();
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

    if (child_pid != pid) {
        /* In this case, child_pid and pid should match */
        fixme(0);
        exit(FATAL_EXIT);
    }

    send_ctrlmsgf("%c %s", done, line);

    return pid;
}

static int ctrlmsg_handler(char type, int val, const char *line, int *rexit) {
    int ret;

    switch (type) {

        case 'B':   /* BYE          */
            set_killpid(-1);
            send_ctrlmsgf("%c %s", done, line);
            ret = 1;
            break;

        case 'D':   /* DETACH       */
            /* repeat the message to cmdline as it is, then close the fd */
            send_ctrlmsgf("%s", line);
            if (ctrl_wfd >= 0) {
                close(ctrl_wfd);
                ctrl_wfd = -1;
            }
            ret = 0;
            break;

        case 'E':   /* EXIT=N       */
            *rexit = val;
            send_ctrlmsgf("%c %s", done, line);
            send_ctrlmsgf("EXIT=%d", val);
            ret = 0;
            break;

        case 'K':   /* KILLPID=N    */
            set_killpid(val);
            send_ctrlmsgf("%c %s", done, line);
            ret = 0;
            break;

        case 'T':   /* TIMEOUT=N    */
            set_timeout(val);
            send_ctrlmsgf("%c %s", done, line);
            send_ctrlmsgf("TIMEOUT=%d", val + EXTRA_TIMEOUT);
            ret = 0;
            break;

        default:
            send_ctrlmsgf("%s", line);
            ret = 0;
            break;
    }

    return ret;
}

static int waitpid_for_a_while(int child) {
    int interval = 100; /* millisecond  */
    int count = HOLDON_DELAY * (1000 / interval);

    while (1) {

        errorpf(-1, 0, "XXX TODO last %d ms", count * interval); // XXX TODO 動作確認が終わったら削除すること

        int ret = waitpid_nohang(child);
        if (ret >= 0)
            return ret;

        if (--count <= 0)
            break;

        usleep(interval * 1000);
    }
    return -1;
}

static int waitpid_nohang(int child) {
    int status = 0;
    int ret = waitpid(child, &status, WNOHANG);
    if (ret < 0) {
        fixme(errno);
        exit(FATAL_EXIT);
    }
    else if (ret == 0) {
        /* child have not yet changed state */
    }
    else if (ret > 0) {
        if (WIFEXITED(status)) {
            ret = WEXITSTATUS(status);
            noticepf("monitoring-target returned exit status %d", ret);
            return ret;
        }
        if (WIFSIGNALED(status)) {
            ret = WTERMSIG(status);
            noticepf("monitoring-target got a signal %d", ret);
            return SIGNAL_EXIT(ret);
        }
        /* fall through */
    }
    return -1;
}

/* vim: set et sw=4 sts=4: */
