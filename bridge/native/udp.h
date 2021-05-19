#pragma once

#include <stdint.h>

typedef struct udp_conn_t udp_conn_t;

typedef struct udp_metadata_t {
    uint8_t src_addr[4];
    uint8_t dst_addr[4];
    uint16_t src_port;
    uint16_t dst_port;
} udp_metadata_t;

udp_conn_t *udp_conn_listen();
void udp_conn_close(udp_conn_t *conn);
void udp_conn_free(udp_conn_t *udp);

int udp_conn_recv(udp_conn_t *conn, udp_metadata_t *metadata, void *buffer, int size);
int udp_conn_sendto(udp_conn_t *conn, udp_metadata_t *metadata, void *buffer, int size);
