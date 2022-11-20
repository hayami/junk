#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>

#include "errorpf.h"
#include "global.h"

char *const *exec_argv = NULL;

int execfunc() {
    send_ctrlmsgf("KILLPID=%d", -getpgid(0));
    send_ctrlmsgf("TIMEOUT=%d", BASE_TIMEOUT);

    /* copy ctrl_wfd to ctrl_kfd (usually ctrl_kfd is 3) and set FD_CLOEXEC
       flag on ctrl_wfd so that it will be closed on successful execvp(). */
    int ret = dup2(ctrl_wfd, ctrl_kfd);
    if (ret == -1) {
        errorpf(errno, "dup2()");
        exit(FATAL_EXIT);
    }
    int flags = fcntl(ctrl_wfd, F_GETFD);
    if (flags == -1) {
        errorpf(errno, "fcntl(F_GETFD)");
        exit(FATAL_EXIT);
    }
    ret = fcntl(ctrl_wfd, F_SETFD, flags | FD_CLOEXEC);
    if (ret == -1) {
        errorpf(errno, "fcntl(F_SETFD)");
        exit(FATAL_EXIT);
    }

    execvp(exec_argv[0], exec_argv);
    errorpf(errno, "execvp(%s)", exec_argv[0]);

    /* Above execvp() has failed. This stage will exit now. Do not
       send signals to me (ctrl_wfd is still available here) */
    send_ctrlmsgf("KILLPID=%d", -1);

    close(ctrl_kfd);
    ctrl_kfd = -1;
    return OTHER_ERROR_EXIT;
}

/* vim: set et sw=4 sts=4: */
