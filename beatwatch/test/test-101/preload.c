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

static pid_t (*libc_fork)(void) = NULL;

static void init()
{
    *(void **)(&libc_fork) = dlsym(RTLD_NEXT, "fork");
    if (!libc_fork) {
        perror("fork()");
        _exit(1);
    }
}

static void fini()
{
}

pid_t fork() {
    static int count = 0;
    Dl_info info;

    if (dladdr(__builtin_return_address(0), &info) == 0) {
        fprintf(stderr, "dladdr(): %s\n", dlerror());
        _exit(1);
    }

    if (info.dli_sname && strcmp(info.dli_sname, "pipe_and_fork") == 0) {
        count++;
        if (count == 1) {
            errno = EAGAIN;
            return -1;
        }
    }

    return (*libc_fork)();
}

/* vim: set et sw=4 sts=4: */
