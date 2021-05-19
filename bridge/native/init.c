#include "init.h"

#include "utils.h"
#include "interface.h"

#include "lwip/tcpip.h"

struct initialize_context {
    mtx_t lock;
    cnd_t cond;
    int initialized;
};

static void tcpip_initialize(void *arg) {
    global_interface_init();

    struct initialize_context *context = (struct initialize_context *) arg;

    WITH_MUTEX_LOCKED(initialize, &context->lock);

    context->initialized = 1;

    cnd_broadcast(&context->cond);
}

UNUSED
void init_lwip() {
    struct initialize_context context = {.initialized = 0};

    mtx_init(&context.lock, mtx_plain);
    cnd_init(&context.cond);

    WITH_MUTEX_LOCKED(initialize, &context.lock);

    tcpip_init(tcpip_initialize, &context);

    while (!context.initialized)
        cnd_wait(&context.cond, &context.lock);
}
