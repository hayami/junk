#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include "error.h"
#include "tls.h"

#define LOCAL	0
#define REMOTE	1

int get_connected_socket(const char *host, const char *port, int type)
{
    struct addrinfo gaihints, *gairet, *ap;
    int ret, sockfd, fd, last_errno = 0;

    memset(&gaihints, 0, sizeof(gaihints));
    gaihints.ai_family = AF_UNSPEC;	/* Allow IPv4 or IPv6 */
    gaihints.ai_socktype = SOCK_STREAM;
    ret = getaddrinfo(host, port, &gaihints, &gairet);
    if (ret != 0) {
        fprintf(stderr, "%s: host=%s, port=%s\n",
                gai_strerror(ret), host, port);
        exit(FATAL_USER);
    }

    for (ap = gairet; ap != NULL; ap = ap->ai_next) {
        sockfd = socket(ap->ai_family, ap->ai_socktype, ap->ai_protocol);
        if (sockfd < 0)
            goto try_next;

        if (type == REMOTE) {
            ret = connect(sockfd, ap->ai_addr, ap->ai_addrlen);
            if (ret != 0)
                goto try_next;
        } else {
            const int enable = 1;
            ret = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
                             &enable, sizeof(enable));
            if (ret != 0)
                goto try_next;

            ret = bind(sockfd, ap->ai_addr, ap->ai_addrlen);
            if (ret != 0)
                goto try_next;
        }

        break;

try_next:
        last_errno = errno;
        if (sockfd >= 0)
            close(sockfd);
    }
    freeaddrinfo(gairet);

    if (ap == NULL) {
        fprintf(stderr, "%s: host=%s, port=%s\n",
                strerror(last_errno), host, port);
        exit(FATAL_USER);
    }

    if (type == REMOTE)
        return sockfd;

    ret = listen(sockfd, 1);
    if (ret != 0) {
        perror("listen()");
        exit(FATAL_SYSTEM);
    }

    struct sockaddr peer_addr;
    socklen_t peer_addrlen = 0;
    memset(&peer_addr, 0, sizeof(peer_addr));
    fd = accept(sockfd, &peer_addr, &peer_addrlen);
    if (fd < 0) {
        perror("accept()");
        exit(FATAL_SYSTEM);
    }

    /* Ignore later connections */
    close(sockfd);

    return fd;
}

int main(int argc, char *argv[])
{
    int fd, rsize;
    char *remote, *port, buf[4096]; 
    TC *tc;

    if (argc != 2) {
        fprintf(stderr, "usage: getroot server[:port]\n");
        exit(1);
    }
    remote = argv[1];

    port = strrchr(remote, ':');
    if (port) {
        *port++ = '\0';
    } else {
        port = "https";
    }

    tls_init();
    tc = tls_client_new(NULL, remote);
    fd = get_connected_socket(remote, port, REMOTE);
    tc->open(tc, fd);

    sprintf(buf,
        "GET / HTTP/1.1"	"\r\n"
        "Host: %s"		"\r\n"
        "Connection: close"	"\r\n"
        "Accept: */*"		"\r\n"
        ""			"\r\n"
        , remote);

    tc->write(tc, buf, strlen(buf));

    do {
        rsize = tc->read(tc, buf, sizeof(buf));
        write(1, buf, rsize);
    } while (rsize > 0);

    tc->close(tc);
    close(fd);
    tls_client_free(tc);

    return 0;
}

/* vim: set et sw=4 sts=4: */
