#pragma once

#include "utils.h"

#include <stdint.h>

typedef struct udp_conn_t udp_conn_t;

EXPORT udp_conn_t *udp_conn_listen();

EXPORT void udp_conn_close(udp_conn_t *conn);

EXPORT void udp_conn_free(udp_conn_t *udp);

EXPORT int udp_conn_recv(udp_conn_t *conn, endpoint_t *endpoint, void *buffer, int size);

EXPORT int udp_conn_sendto(udp_conn_t *conn, endpoint_t *endpoint, void *buffer, int size);
