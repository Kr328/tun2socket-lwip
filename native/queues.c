#include "queues.h"

int pbuf_queue_append(pbuf_queue_t *queue, struct pbuf *in[], int size) {
    for (int i = 0; i < size; i++) {
        int tail = queue->tail;

        if (queue->data[tail] != NULL) {
            pbuf_free(queue->data[tail]);
        }

        queue->data[tail] = in[i];

        queue->tail = (tail + 1) % PBUF_QUEUE_LENGTH;

        if (queue->full) {
            queue->head = queue->tail;
        }

        queue->full = (queue->head == queue->tail);
    }

    return size;
}

int pbuf_queue_length(pbuf_queue_t *queue) {
    int size = (queue->tail - queue->head + PBUF_QUEUE_LENGTH) % PBUF_QUEUE_LENGTH;

    if (size == 0 && queue->full)
        return PBUF_QUEUE_LENGTH;

    return size;
}

int pbuf_queue_pop(pbuf_queue_t *queue, struct pbuf *out[], int size) {
    int i;

    for (i = 0; i < size; i++) {
        if (queue->head == queue->tail && !queue->full)
            break;

        int head = queue->head;

        out[i] = queue->data[head];

        queue->data[head] = NULL;

        queue->head = (head + 1) % PBUF_QUEUE_LENGTH;
        queue->full = 0;
    }

    return i;
}
