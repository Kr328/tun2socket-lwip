#include "device.h"

#include "utils.h"
#include "interface.h"
#include "queues.h"

#include "lwip/tcpip.h"

#include <string.h>
#include <malloc.h>

struct device_context_t {
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
    device_context_t *context = (device_context_t *) ctx;

    struct pbuf *array[DEVICE_BUFFER_ARRAY_SIZE];
    int size;

    {
        WITH_MUTEX_LOCKED(lock, &context->tx_mutex);

        context->tx_polling = 0;

        size = pbuf_queue_pop(&context->tx, array, DEVICE_BUFFER_ARRAY_SIZE);
    }

    for (int i = 0; i < size; i++) {
        global_interface_inject_packet(array[i]);
    }
}

static void if_output(void *context, struct pbuf *p) {
    device_context_t *ctx = (device_context_t *) context;

    WITH_MUTEX_LOCKED(lock, &ctx->rx_mutex);

    pbuf_queue_append(&ctx->rx, &p, 1);

    pthread_cond_signal(&ctx->rx_cond);
}

EXPORT
device_context_t *new_device_context(int mtu) {
    device_context_t *ctx = (device_context_t *) malloc(sizeof(device_context_t));

    memset(ctx, 0, sizeof(device_context_t));

    pthread_mutex_init(&ctx->rx_mutex, NULL);
    pthread_mutex_init(&ctx->tx_mutex, NULL);

    pthread_cond_init(&ctx->rx_cond, NULL);

    ctx->mtu = mtu;

    return ctx;
}

static void do_device_attach(void *arg) {
    device_context_t *ctx = arg;

    global_interface_attach_device(&if_output, ctx, ctx->mtu);
}

EXPORT
void device_attach(device_context_t *ctx) {
    tcpip_callback(&do_device_attach, ctx);
}

static void do_device_close(void *arg) {
    device_context_t *ctx = arg;

    global_interface_attach_device(NULL, NULL, -1);

    WITH_MUTEX_LOCKED(rx, &ctx->rx_mutex);
    WITH_MUTEX_LOCKED(tx, &ctx->tx_mutex);

    ctx->closed = 1;

    pthread_cond_broadcast(&ctx->rx_cond);
}

EXPORT
void device_close(device_context_t *ctx) {
    tcpip_callback(&do_device_close, ctx);
}

EXPORT
void device_free(device_context_t *ctx) {
    free(ctx);
}

EXPORT
int device_read_rx_packet(device_context_t *ctx, void *buffer, int size) {
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
int device_write_tx_packet(device_context_t *ctx, void *buffer, int size) {
    struct pbuf *target = pbuf_alloc(PBUF_IP, size, PBUF_POOL);

    pbuf_take(target, buffer, size);

    WITH_MUTEX_LOCKED(lock, &ctx->tx_mutex);

    pbuf_queue_append(&ctx->tx, &target, 1);

    if (!ctx->tx_polling) {
        if (tcpip_try_callback(&poll_tx, ctx) == ERR_OK) {
            ctx->tx_polling = 1;
        }
    }

    return size;
}

