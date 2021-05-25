#pragma once

#include "utils.h"

#include <stdint.h>

typedef struct tcp_listener_t tcp_listener_t;
typedef struct tcp_conn_t tcp_conn_t;

EXPORT tcp_listener_t *tcp_listener_listen();
EXPORT tcp_conn_t *tcp_listener_accept(tcp_listener_t *listener);
EXPORT void tcp_listener_close(tcp_listener_t *listener);
EXPORT void tcp_listener_free(tcp_listener_t *listener);

EXPORT int tcp_conn_read(tcp_conn_t *conn, void *data, int length);
EXPORT int tcp_conn_write(tcp_conn_t *conn, void *data, int length);
EXPORT void tcp_conn_local_addr(tcp_conn_t *conn, uint8_t addr[4], uint16_t *port);
EXPORT void tcp_conn_remote_addr(tcp_conn_t *conn, uint8_t addr[4], uint16_t *port);
EXPORT void tcp_conn_close(tcp_conn_t *conn);
EXPORT void tcp_conn_free(tcp_conn_t *conn);
