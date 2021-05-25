#pragma once

#include "utils.h"

#include <stdint.h>

typedef struct udp_conn_t udp_conn_t;

typedef struct udp_metadata_t {
    uint8_t src_addr[4];
    uint8_t dst_addr[4];
    uint16_t src_port;
    uint16_t dst_port;
} udp_metadata_t;

EXPORT udp_conn_t *udp_conn_listen();
EXPORT void udp_conn_close(udp_conn_t *conn);
EXPORT void udp_conn_free(udp_conn_t *udp);
EXPORT int udp_conn_recv(udp_conn_t *conn, udp_metadata_t *metadata, void *buffer, int size);
EXPORT int udp_conn_sendto(udp_conn_t *conn, udp_metadata_t *metadata, void *buffer, int size);
