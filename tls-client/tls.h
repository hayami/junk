typedef struct tls_client_ctx TCX;
typedef struct tls_client TC;

struct tls_client_ctx_private;
struct tls_client_ctx {
    struct tls_client_ctx_private *const p;
};

struct tls_client_private;
struct tls_client {
    void (*const open)(TC *client, int socket);
    void (*const close)(TC *client);
    int (*const read)(TC *client, void *buf, int size);
    int (*const write)(TC *client, const void *buf, int size);
    struct tls_client_private *const p;
};

void tls_init();

TCX *tls_client_ctx_new(void);
void tls_client_ctx_free(TCX *ctx);

TC *tls_client_new(TCX *ctx, const char *servername);
void tls_client_free(TC *client);

/* vim: set et sw=4 sts=4: */
