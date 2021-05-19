#include "link.h"

#include "utils.h"
#include "interface.h"
#include "queues.h"

#include "lwip/tcpip.h"

#include <string.h>
#include <stdlib.h>

struct link_t {
    struct pbuf_queue_t rx;
    struct pbuf_queue_t tx;

    pthread_mutex_t rx_mutex;
    pthread_cond_t rx_cond;

    pthread_mutex_t tx_mutex;
    int tx_polling;

    int closed;
    int mtu;
};

static void poll_tx(void *ctx) {
    link_t *context = (link_t *) ctx;

    struct pbuf *array[32];
    int size;

    {
        WITH_MUTEX_LOCKED(lock, &context->tx_mutex);

        context->tx_polling = 0;

        size = pbuf_queue_pop(&context->tx, array, 32);
    }

    for (int i = 0; i < size; i++) {
        global_interface_inject_packet(array[i]);
    }
}

static void if_output(void *context, struct pbuf *p) {
    link_t *ctx = (link_t *) context;

    WITH_MUTEX_LOCKED(lock, &ctx->rx_mutex);

    pbuf_queue_append(&ctx->rx, &p, 1);

    pthread_cond_signal(&ctx->rx_cond);
}

EXPORT
link_t *link_attach(int mtu) {
    WITH_LWIP_LOCKED();

    if (global_interface_is_attached())
        return NULL;

    link_t *ctx = (link_t *) malloc(sizeof(link_t));

    memset(ctx, 0, sizeof(link_t));

    pthread_mutex_init(&ctx->rx_mutex, NULL);
    pthread_mutex_init(&ctx->tx_mutex, NULL);

    pthread_cond_init(&ctx->rx_cond, NULL);

    ctx->mtu = mtu;

    global_interface_attach_device(&if_output, ctx, ctx->mtu);

    return ctx;
}

EXPORT
void link_close(link_t *ctx) {
    WITH_LWIP_LOCKED();

    global_interface_attach_device(NULL, NULL, DEFAULT_MTU);

    WITH_MUTEX_LOCKED(rx, &ctx->rx_mutex);
    WITH_MUTEX_LOCKED(tx, &ctx->tx_mutex);

    ctx->closed = 1;

    pthread_cond_broadcast(&ctx->rx_cond);
}

EXPORT
void link_free(link_t *ctx) {
    free(ctx);
}

EXPORT
int link_read(link_t *ctx, void *buffer, int size) {
    struct pbuf *source = NULL;

    {
        WITH_MUTEX_LOCKED(lock, &ctx->rx_mutex);

        while (pbuf_queue_length(&ctx->rx) == 0) {
            if (ctx->closed)
                return -1;

            pthread_cond_wait(&ctx->rx_cond, &ctx->rx_mutex);
        }

        pbuf_queue_pop(&ctx->rx, &source, 1);
    }

    if (source == NULL)
        return 0;

    if (size >= source->tot_len) {
        pbuf_copy_partial(source, buffer, source->tot_len, 0);

        size = source->tot_len;
    } else {
        size = 0;
    }

    pbuf_free(source);

    return size;
}

EXPORT
int link_write(link_t *ctx, void *buffer, int size) {
    struct pbuf *target = pbuf_alloc(PBUF_IP, size, PBUF_POOL);

    pbuf_take(target, buffer, size);

    WITH_MUTEX_LOCKED(lock, &ctx->tx_mutex);

    if (ctx->closed) {
        pbuf_free(target);

        return -1;
    }

    pbuf_queue_append(&ctx->tx, &target, 1);

    if (!ctx->tx_polling) {
        if (tcpip_try_callback(&poll_tx, ctx) == ERR_OK) {
            ctx->tx_polling = 1;
        }
    }

    return size;
}

