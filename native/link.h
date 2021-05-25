#pragma once

#include "utils.h"

#include <stdint.h>
#include <stddef.h>
#include <pthread.h>

typedef struct link_t link_t;

EXPORT link_t *link_attach(int mtu);
EXPORT void link_close(link_t *ctx);
EXPORT void link_free(link_t *ctx);
EXPORT int link_read(link_t *ctx, void *buffer, int size);
EXPORT int link_write(link_t *ctx, void *buffer, int size);