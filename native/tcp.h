#pragma once

#include "utils.h"

#include <stdint.h>

#define TCP_CONNECTION_SIZE 512

#define EVENT_READABLE 1u
#define EVENT_WRITABLE 2u

typedef struct tcp_poller_t tcp_poller_t;

EXPORT tcp_poller_t *new_tcp_poller();

EXPORT void tcp_poller_close(tcp_poller_t *poller);

EXPORT void tcp_poller_free(tcp_poller_t *poller);

EXPORT int tcp_poller_accept(tcp_poller_t *poller, uint16_t *index, endpoint_t *endpoint);

EXPORT int tcp_poller_events(tcp_poller_t *poller, uint32_t *events, int size);

EXPORT int tcp_poller_read_index(tcp_poller_t *poller, uint16_t index, void *buffer, int size);

EXPORT int tcp_poller_write_index(tcp_poller_t *poller, uint16_t index, const void *buffer, int size);

EXPORT int tcp_poller_close_index(tcp_poller_t *poller, uint16_t index);
