#include "init.h"

#include "utils.h"
#include "interface.h"

#include "lwip/tcpip.h"

struct initialize_context {
    pthread_mutex_t lock;
    pthread_cond_t cond;
    int initialized;
};

static void tcpip_initialize(void *arg) {
    global_interface_init();

    struct initialize_context *context = (struct initialize_context *) arg;

    WITH_MUTEX_LOCKED(initialize, &context->lock);

    context->initialized = 1;

    pthread_cond_broadcast(&context->cond);
}

EXPORT
void init_lwip() {
    struct initialize_context context = {.initialized = 0};

    pthread_mutex_init(&context.lock, NULL);
    pthread_cond_init(&context.cond, NULL);

    WITH_MUTEX_LOCKED(initialize, &context.lock);

    tcpip_init(tcpip_initialize, &context);

    while (!context.initialized)
        pthread_cond_wait(&context.cond, &context.lock);
}
