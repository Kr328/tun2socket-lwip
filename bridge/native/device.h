#pragma once

#include "utils.h"

#include <stdint.h>
#include <stddef.h>
#include <pthread.h>

#define DEVICE_BUFFER_ARRAY_SIZE 256

typedef struct device_context_t device_context_t;

typedef struct device_buffer_t {
    void *data;
    int length;
} device_buffer_t;

typedef struct device_buffer_array_t {
    device_buffer_t buffers[DEVICE_BUFFER_ARRAY_SIZE];
} device_buffer_array_t;

EXPORT device_context_t *new_device_context(int mtu);
EXPORT void device_attach(device_context_t *ctx);
EXPORT void device_close(device_context_t *ctx);
EXPORT void device_free(device_context_t *ctx);

EXPORT int device_read_rx_packets(device_context_t *ctx, struct device_buffer_array_t *buffers);
EXPORT int device_write_tx_packets(device_context_t *ctx, struct device_buffer_array_t *buffers, int size);