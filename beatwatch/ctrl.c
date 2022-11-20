#ifdef __linux__
#define _GNU_SOURCE     /* for vasprintf() */
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>

#include "errorpf.h"
#include "global.h"

int ctrl_rfd = -1;
int ctrl_wfd = -1;
FILE *ctrl_logfp = NULL;

static int recv_line(const char **rline);
static int send_ctrlmsgf_internal(const char *fmt, va_list ap);
static const char *logtime(void);

int recv_ctrlmsg(char *rtype, int *rval, const char **rline) {
    *rtype = '\0';
    *rval = 0;
    *rline = NULL;

    const char *line;
    int len = recv_line(&line);
    if (len < 0) {
        /* interrupted */
        return -1;
    }
    if (len == 0) {
        /* got an EOF */
        return 0;
    }
    *rline = line;

    int tmpint;
    if (strcmp(line, "BYE") == 0) {
        *rtype = *line;
        *rval = 0;
    }
    else if (strcmp(line, "DETACH") == 0) {
        *rtype = *line;
        *rval = 0;
    }
    else if (sscanf(line, "EXIT=%d", &tmpint) == 1) {
        *rtype = *line;
        *rval = tmpint;
    }
    else if (sscanf(line, "KILLPID=%d", &tmpint) == 1) {
        *rtype = *line;
        *rval = tmpint;
    }
    else if (sscanf(line, "TIMEOUT=%d", &tmpint) == 1) {
        *rtype = *line;
        *rval = tmpint;
    }
    return len;
}

static int recv_line(const char **rline) {
    static char buf[256];
    static char *nextp = buf;
    static int nread = 0;

    *rline = NULL;

    if (nextp > buf && nread > 0) {
        memmove(buf, nextp, nread);
    }

    int n = 0;
    do {
        nread += n;

        /* make sure '\n' has been read */
        int odd = nread;
        nextp = buf;
        while (odd > 0) {
            if (*nextp == '\n') {
                *nextp++ = '\0';
                nread = --odd;
                *rline = buf;
                return (nextp - buf) - 1;
            }
            odd--;
            nextp++;
        }

        if (sizeof(buf) - nread <= 0) {
            errorpf(0, "too long control message received");
            exit(OTHER_ERROR_EXIT);
        }
        n = read(ctrl_rfd, buf + nread, sizeof(buf) - nread);

    } while (n > 0);

    if (n == 0) {
        /* got an EOF */
        return 0;
    }

    /* n < 0 */
    if (errno != EINTR) {
        /* got an unexpected errno */
        fixme(errno);
        exit(FATAL_EXIT);
    }
    /* interrupted by a signal before any data was read */
    return -1;
}

void send_ctrlmsgf(const char *fmt, ...) {
    int ret;

    va_list ap;
    va_start(ap, fmt);
    ret = send_ctrlmsgf_internal(fmt, ap);
    va_end(ap);

    if (ret > 0) {
        return;
    }
    else if (ret == 0) {
        errorpf(0, "vasprintf()");
        exit(FATAL_EXIT);
    }
    else if (ret < 0) {
        ret = -ret;

        if (ret == EPIPE) {
            /* reading end has closed the fd */
            close(ctrl_wfd);
            ctrl_wfd = -1;
            errorpf(ret, "control channel is closed");
            exit(OTHER_ERROR_EXIT);
        }
        else {
            /* got an unexpected errno */
            close(ctrl_wfd);
            ctrl_wfd = -1;
            fixme(ret);
            exit(FATAL_EXIT);
        }
    }
}

void send_ctrlmsgf_without_error_handling(const char *fmt, ...) {
    if (ctrl_wfd < 0)
        return;

    va_list ap;
    va_start(ap, fmt);
    send_ctrlmsgf_internal(fmt, ap);
    va_end(ap);

    return;
}

static int send_ctrlmsgf_internal(const char *fmt, va_list ap) {
    char *str;
    int ret;

    ret = vasprintf(&str, fmt, ap);
    if (ret < 0 || str == NULL)
        return 0;

    ret = 1;

    if (ctrl_wfd >= 0) {
        char *wptr = str;
        int len = strlen(str);
        int n, wsize = len + 1;

        *(str + len) = '\n';    /* once, overwrite '\0' with '\n' */

        do {
            n = write(ctrl_wfd, wptr, wsize);
            if (n < 0) {
                if (errno == EINTR) {
                    /* try again */
                    continue;
                }
                ret = -errno;
                break;

            } else if (n == 0) {
                /* write() returns 0 when nothing was written */
                continue;

            } else if (n > 0) {
                wptr += n;
                wsize -= n;
                if (wsize < 0) {
                    errorpf(0, "BUG at %s:%d", __FILE__, __LINE__);
                    exit(FATAL_EXIT);
                }
            }
        } while (wsize > 0);

        *(str + len) = '\0';    /* restore the '\0' above */
    }
    if (ctrl_logfp) {
        fprintf(ctrl_logfp, "%s\t%s\n", logtime(), str);
        fflush(ctrl_logfp);
    }
    free(str);
    return ret;
}

static const char *logtime() {
    static char fmt[64];
    static char retbuf[64];
    int ret;
    struct tm* tm;
    struct timeval tv;

    ret = gettimeofday(&tv, NULL);
    if (ret != 0)
        goto fail;

    tm = localtime(&tv.tv_sec);
    if (tm == NULL)
        goto fail;

    ret = strftime(fmt, sizeof(fmt), "%F %T.%%03d (%z)", tm);
    if (ret <= 0)
        goto fail;

    ret = snprintf(retbuf, sizeof(retbuf), fmt, tv.tv_usec / 1000);
    if (ret <= 0 || ret >= (int) sizeof(retbuf))
        goto fail;

    return retbuf;

fail:
    strcpy(retbuf, "0000-00-00 00:00:00.000 (+0000)");
    return retbuf;
}

/* vim: set et sw=4 sts=4: */
