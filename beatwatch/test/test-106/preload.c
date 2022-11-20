#ifdef __linux__
#define _GNU_SOURCE     /* for RTLD_NEXT */
#endif

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dlfcn.h>

static void init(void) __attribute__((constructor));
static void fini(void) __attribute__((destructor));

static int (*libc_setpgid)(pid_t pid, pid_t pgid) = NULL;

static void init()
{
    *(void **)(&libc_setpgid) = dlsym(RTLD_NEXT, "setpgid");
    if (!libc_setpgid) {
        perror("setpgid()");
        _exit(1);
    }
}

static void fini()
{
}

int setpgid(pid_t pid, pid_t pgid) {
    errno = EPERM;
    return -1;

    /* NOT REACHED */
    return (*libc_setpgid)(pid, pgid);
}

/* vim: set et sw=4 sts=4: */
