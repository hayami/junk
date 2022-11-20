#include <stdlib.h>
#include <unistd.h>

#include "errorpf.h"
#include "global.h"

int pipe_and_fork() {
    int fd[2];

    int ret = pipe(fd);
    if (ret != 0) {
        errorpf(errno, "pipe()");
        exit(FATAL_EXIT);
    }

    int child_pid = fork();
    if (child_pid < 0) {
        errorpf(errno, "fork()");
        exit(FATAL_EXIT);
    }

    if (child_pid == 0) {
        close(fd[0]);
        if (ctrl_wfd >= 0)
            close(ctrl_wfd);
        ctrl_wfd = fd[1];
        return 0;
    }

    /* child_pid > 0 */
    close(fd[1]);
    if (ctrl_rfd >= 0)
        close(ctrl_rfd);
    ctrl_rfd = fd[0];
    return child_pid;
}

/* vim: set et sw=4 sts=4: */
