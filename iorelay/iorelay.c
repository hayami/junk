#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <sys/param.h>
#include <pthread.h>

#include "iorelay.h"

#ifndef IOBUFSIZE
#define IOBUFSIZE	(1 * 4096)	/* 4 KiB */
#endif

#define NWFDS		8

static int quit = 0;	/* not implemented */
static void *mainloop(void *arg);
static void selectwait(int rfd, int wfd, int efd);

struct io {
    struct {
        int fd;
        int can_close;
    } r;
    struct {
        int nfds;
        int fd[NWFDS];
        int can_close[NWFDS];
    } w;
    struct {
        char *ptr;
        int size;
    } buf;
};

int _vniorelay(int rfd, int nwfds, ...)
{
    va_list ap;
    int i;
    int wfd[NWFDS] = { 0, };

    if (nwfds < 1 || nwfds > NWFDS)
        return EINVAL;

    va_start(ap, nwfds);
    for (i = 0; i < nwfds; i++) {
        wfd[i] = va_arg(ap, int);
    }
    va_end(ap);

    return niorelay(rfd, nwfds, wfd);
}

int niorelay(int rfd, int nwfds, int wfd[])
{
    int i, err;
    struct io *io;
    pthread_t th;
    pthread_attr_t attr;

    if (nwfds < 1 || nwfds > NWFDS)
        return EINVAL;

    io = malloc(sizeof(struct io));
    if (io == NULL)
        return ENOMEM;
    memset(io, 0, sizeof(struct io));

    if (rfd < 0) {
        io->r.fd = ~rfd;
        io->r.can_close = 0;
    } else {
        io->r.fd = rfd;
        io->r.can_close = 1;
    }

    io->w.nfds = nwfds;
    for (i = 0; i < nwfds; i++) {
        if (wfd[i] < 0) {
            io->w.fd[i] = ~wfd[i];
            io->w.can_close[i] = 0;
        } else {
            io->w.fd[i] = wfd[i];
            io->w.can_close[i] = 1;
        }
    }

    io->buf.ptr = malloc(IOBUFSIZE);
    if (io->buf.ptr == NULL) {
        free(io);
        return ENOMEM;
    }
    io->buf.size = IOBUFSIZE;
    memset(io->buf.ptr, 0, IOBUFSIZE);

    err = pthread_attr_init(&attr);
    if (err != 0)
        return err;

    err = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    if (err != 0)
        return err;

    err = pthread_create(&th, &attr, mainloop, (void *) io);
    if (err != 0)
        return err;

    return 0;
}

static void *mainloop(void *arg)
{
    struct io *io = (struct io *) arg;
    int i, n, rsize, wsize, nwactive = io->w.nfds;;
    char *ptr;

    while (1) {
        do {
            n = read(io->r.fd, io->buf.ptr, io->buf.size);
            if (n < 0) {
                if (errno == EINTR) {
                    if (quit)
                        goto quit;

                    /* try again */
                }
                else if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    if (quit)
                        goto quit;

                    /* non-blocking I/O operations are required */
                    selectwait(io->r.fd, -1, -1);
                    if (quit)
                        goto quit;

                    /* anyway, try again */
                }
            }
            else if (n == 0) {
                /* got an EOF */
                goto done;
            }
        } while (n <= 0);
        rsize = n;

        for (i = 0; i < io->w.nfds; i++) {
            if (io->w.fd[i] < 0)
                 continue;

            ptr = io->buf.ptr;
            wsize = rsize;
            do {
                n = write(io->w.fd[i], ptr, wsize);
                if (n < 0) {
                    if (errno == EINTR) {
                        if (quit)
                            goto quit;

                        /* try again */
                    }
                    else if (errno == EPIPE) {
                        /* reading end has closed */

                        if (io->w.can_close[i])
                            close(io->w.fd[i]);
                        io->w.fd[i] = -1;

                        if (--nwactive <= 0)
                            goto done;

                        wsize = 0;
                    }
                    else if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        if (quit)
                            goto quit;

                        /* non-blocking I/O operations are required */
                        selectwait(-1, io->w.fd[i], -1);
                        if (quit)
                            goto quit;

                        /* anyway, try again */
                    }
                }
                else if (n > 0) {
                    ptr += n;
                    wsize -= n;
                }
                /* n == 0 indicates nothing was written */
            } while (wsize > 0);
        }
    }

quit:	/* not implemented */

done:
    if (io->r.can_close)
         close(io->r.fd);

    for (i = 0; i < io->w.nfds; i++)
        if (io->w.can_close[i] && io->w.fd[i] >= 0)
             close(io->w.fd[i]);

    free(io->buf.ptr);
    free(io);
    return NULL;
}

static void selectwait(int rfd, int wfd, int efd)
{
    int nfds;
    fd_set rfds, wfds, efds;
    struct timeval timeout;

    nfds = MAX(MAX(rfd, wfd), efd) + 1;

    FD_ZERO(&rfds); if (rfd >= 0) FD_SET(rfd, &rfds);
    FD_ZERO(&wfds); if (wfd >= 0) FD_SET(wfd, &wfds);
    FD_ZERO(&efds); if (efd >= 0) FD_SET(efd, &efds);

    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

    select(nfds, &rfds, &wfds, &efds, &timeout);
    /* The select() returns the number of ready descriptors, or -1 if
     an error occurred. If the time limit expires, select() returns 0. */
}

/* vim: set et sw=4 sts=4: */
