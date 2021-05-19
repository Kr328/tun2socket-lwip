#pragma once

#include "lwip/pbuf.h"

#define PBUF_QUEUE_LENGTH 256

typedef struct pbuf_queue_t {
    int head;
    int tail;
    int full;

    struct pbuf *data[PBUF_QUEUE_LENGTH];
} pbuf_queue_t;

int pbuf_queue_append(pbuf_queue_t *queue, struct pbuf *in[], int size);
int pbuf_queue_length(pbuf_queue_t *queue);
int pbuf_queue_pop(pbuf_queue_t *queue, struct pbuf *out[], int size);
