#pragma once

#include <stdint.h>

typedef struct tcp_listener_t tcp_listener_t;
typedef struct tcp_conn_t tcp_conn_t;

tcp_listener_t *tcp_listener_listen();
tcp_conn_t *tcp_listener_accept(tcp_listener_t *listener);
void tcp_listener_close(tcp_listener_t *listener);
void tcp_listener_free(tcp_listener_t *listener);

int tcp_conn_read(tcp_conn_t *conn, void *data, int length);
int tcp_conn_write(tcp_conn_t *conn, void *data, int length);
int tcp_conn_local_addr(tcp_conn_t *conn, uint8_t addr[4], uint16_t *port);
int tcp_conn_remote_addr(tcp_conn_t *conn, uint8_t addr[4], uint16_t *port);
void tcp_conn_close(tcp_conn_t *conn);
void tcp_conn_free(tcp_conn_t *conn);
