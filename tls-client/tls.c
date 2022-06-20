#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/x509v3.h>

#include "error.h"
#include "tls.h"

struct tls_client_ctx_private {
    SSL_CTX *ctx;
};

struct tls_client_private {
    SSL_CTX *ctx;
    int shared_ctx;
    SSL *state;
    int socket;
};

static void tls_client_open(TC *tc, int socket);
static void tls_client_close(TC *tc);
static int tls_client_read(TC *tc, void *buf, int size);
static int tls_client_write(TC *tc, const void *buf, int size);
static int tls_client_verify(int, X509_STORE_CTX *);
static void tls_client_error_exit(int exitcode);

void tls_init() {
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();
    ERR_load_crypto_strings();
}

TCX *tls_client_ctx_new() {
    const SSL_METHOD *method;

    TCX *tcx = malloc(sizeof(*tcx));
    TCX tcx_tmp = {
        .p = malloc(sizeof(*tcx->p))
    };

    if (tcx == NULL || tcx_tmp.p == NULL) {
        perror("ERROR: malloc()");
        exit(FATAL_SYSTEM);
    }
    memcpy(tcx, &tcx_tmp, sizeof(*tcx));
    memset(tcx->p, 0, sizeof(*tcx->p));

    method = TLS_client_method();
    tcx->p->ctx = SSL_CTX_new(method);
    if (tcx->p->ctx == NULL) {
        tls_client_error_exit(FATAL_SYSTEM);
    }

    return tcx;
}

void tls_client_ctx_free(TCX *tcx) {
    if (tcx) {
        if (tcx->p) {
            if (tcx->p->ctx)
                SSL_CTX_free(tcx->p->ctx);
            free(tcx->p);
        }
        free(tcx);
    }
    return;
}

TC *tls_client_new(TCX *tcx, const char *servername) {
    int ret;
    const SSL_METHOD *method;
    X509_VERIFY_PARAM *vp;
    TC *tc = malloc(sizeof(*tc));
    TC tc_tmp = {
        /* The following initialization method requires C99 or GNU gcc. */
        .open = tls_client_open,
        .close = tls_client_close,
        .read = tls_client_read,
        .write = tls_client_write,
        .p = malloc(sizeof(*tc->p))
    };

    if (tc == NULL || tc_tmp.p == NULL) {
        perror("ERROR: malloc()");
        exit(FATAL_SYSTEM);
    }
    memcpy(tc, &tc_tmp, sizeof(*tc));
    memset(tc->p, 0, sizeof(*tc->p));

    if (tcx) {
        if (tcx->p && tcx->p->ctx) {
            tc->p->shared_ctx = 1;
            tc->p->ctx = tcx->p->ctx;
        } else {
            fprintf(stderr, "ERROR: internal error at %s:%d\n", __FILE__, __LINE__);
            exit(FATAL_INTERNAL);
        }
    } else {
        tc->p->shared_ctx = 0;
        method = TLS_client_method();
        tc->p->ctx = SSL_CTX_new(method);
        if (tc->p->ctx == NULL) {
            tls_client_error_exit(FATAL_SYSTEM);
        }
    }

    /* Setting 0 enable up to the highest protocol version */
    SSL_CTX_set_max_proto_version(tc->p->ctx, 0);
    SSL_CTX_set_min_proto_version(tc->p->ctx, TLS1_2_VERSION);

    ret = SSL_CTX_set_default_verify_paths(tc->p->ctx);
    if (ret == 0) {
        tls_client_error_exit(FATAL_SYSTEM);
    }
    SSL_CTX_set_verify(tc->p->ctx, SSL_VERIFY_PEER, tls_client_verify);

    /* create a new TLS connection state */
    tc->p->state = SSL_new(tc->p->ctx);
    if (tc->p->state == NULL) {
        tls_client_error_exit(FATAL_SYSTEM);
    }

    /* set hostname varidation flags */
    vp = SSL_get0_param(tc->p->state);
    X509_VERIFY_PARAM_set_hostflags(
        vp, X509_CHECK_FLAG_ALWAYS_CHECK_SUBJECT
        //| X509_CHECK_FLAG_NO_WILDCARDS
          | X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS
        //| X509_CHECK_FLAG_MULTI_LABEL_WILDCARDS
          | X509_CHECK_FLAG_SINGLE_LABEL_SUBDOMAINS
        //| X509_CHECK_FLAG_NEVER_CHECK_SUBJECT
    );
    if (X509_VERIFY_PARAM_set1_host(vp, servername, 0) == 0) {
        tls_client_error_exit(FATAL_SYSTEM);
    }

    /* use TLS SNI extension to set hostname */
    ret = SSL_set_tlsext_host_name(tc->p->state, servername);
    if (ret == 0) {
        tls_client_error_exit(FATAL_SYSTEM);
    }

    return tc;
}

void tls_client_free(TC *tc) {
    if (tc) {
        if (tc->p) {
            if (tc->p->state)
                SSL_free(tc->p->state);
            if (tc->p->shared_ctx == 0 && tc->p->ctx)
                SSL_CTX_free(tc->p->ctx);
            free(tc->p);
        }
        free(tc);
    }
    return;
}

void tls_client_open(TC *tc, int socket) {
    int ret;

    /* attach the socket descriptor */
    ret = SSL_set_fd(tc->p->state, socket);
    if (ret == 0) {
        tls_client_error_exit(FATAL_SYSTEM);
    }
    tc->p->socket = socket;

    /* TLS connect */
    ret = SSL_connect(tc->p->state);
    if (ret != 1) {
        tls_client_error_exit(FATAL_NETWORK);
    }

    return;
}

static void tls_client_close(TC *tc) {
    if (tc && tc->p && tc->p->state) {
        SSL_shutdown(tc->p->state);
        SSL_free(tc->p->state);
        tc->p->state = NULL;
    }
    return;
}

static int tls_client_read(TC *tc, void *buf, int size) {
    return SSL_read(tc->p->state, buf, size);
}

static int tls_client_write(TC *tc, const void *buf, int size) {
    return SSL_write(tc->p->state, buf, size);
}

static int tls_client_verify(int preverified, X509_STORE_CTX *ctx) {
    return preverified;
}

static void tls_client_error_exit(int exitcode) {
#ifdef DEBUG
    ERR_print_errors_fp(stderr);
#else
    int i, e;

    for (i = 0; (e = ERR_get_error()) != 0; i++) {
        fprintf(stderr, "ERROR: %s\n", ERR_reason_error_string(e));
        if (i >= 8) {
            fprintf(stderr, "ERROR: too many errors emitted, stopping now\n");
            break;
        }
    }
    if (i == 0) {
        fprintf(stderr, "ERROR: (no error codes)\n");
    }
#endif
    exit(exitcode);
}

/* vim: set et sw=4 sts=4: */
