#include "tcp.h"

#include "interface.h"

#include "lwip/api.h"
#include "lwip/sockets.h"

#include <stdio.h>

struct tcp_listener_t {
    struct netconn *conn;
};

struct tcp_conn_t {
    struct netconn *conn;

    ip_addr_t local;
    ip_addr_t remote;
    uint16_t local_port;
    uint16_t remote_port;

    struct netbuf *pending;
    int offset;
};

EXPORT
tcp_listener_t *tcp_listener_listen() {
    struct netconn *conn = netconn_new(NETCONN_TCP);

    if (netconn_bind(conn, IP4_ADDR_ANY, TCP_ACCEPT_ANY_PORT) != ERR_OK)
        goto abort;

    if (netconn_bind_if(conn, netif_get_index(global_interface_get())) != ERR_OK)
        goto abort;

    if (netconn_listen_with_backlog(conn, TCP_DEFAULT_LISTEN_BACKLOG) != ERR_OK)
        goto abort;

    struct tcp_listener_t *listener = malloc(sizeof(struct tcp_listener_t));

    listener->conn = conn;

    return listener;

    abort:
    if (conn != NULL) {
        netconn_delete(conn);
    }

    return NULL;
}

EXPORT
tcp_conn_t *tcp_listener_accept(tcp_listener_t *listener) {
    struct netconn *new_conn = NULL;

    while (1) {
        if (netconn_accept(listener->conn, &new_conn) != ERR_OK) {
            break;
        }

        ip_addr_t local;
        ip_addr_t remote;
        uint16_t local_port;
        uint16_t remote_port;

        if (netconn_getaddr(new_conn, &local, &local_port, 0) != ERR_OK) {
            netconn_delete(new_conn);

            continue;
        }

        if (netconn_getaddr(new_conn, &remote, &remote_port, 1) != ERR_OK) {
            netconn_delete(new_conn);

            continue;
        }

        tcp_conn_t *conn = malloc(sizeof(tcp_conn_t));

        conn->conn = new_conn;
        conn->pending = NULL;
        conn->offset = 0;
        conn->local = local;
        conn->remote = remote;
        conn->local_port = local_port;
        conn->remote_port = remote_port;

        return conn;
    }

    return NULL;
}

EXPORT
void tcp_listener_close(tcp_listener_t *listener) {
    netconn_close(listener->conn);
    netconn_prepare_delete(listener->conn);
}

EXPORT
void tcp_listener_free(tcp_listener_t *listener) {
    netconn_delete(listener->conn);

    free(listener);
}

EXPORT
int tcp_conn_read(tcp_conn_t *conn, void *data, int length) {
    if (conn->pending != NULL) {
        int copied = netbuf_copy_partial(conn->pending, data, length, conn->offset);

        conn->offset += copied;

        if (conn->offset >= netbuf_len(conn->pending)) {
            netbuf_free(conn->pending);
            netbuf_delete(conn->pending);

            conn->pending = NULL;
            conn->offset = 0;
        }

        return copied;
    }

    struct netbuf *buf;

    if (netconn_recv(conn->conn, &buf) != ERR_OK)
        return -1;

    int copied = netbuf_copy_partial(buf, data, length, 0);

    if (copied < netbuf_len(buf)) {
        conn->pending = buf;
        conn->offset = copied;
    } else {
        netbuf_free(buf);
        netbuf_delete(buf);
    }

    return copied;
}

EXPORT
int tcp_conn_write(tcp_conn_t *conn, void *data, int length) {
    if (netconn_write(conn->conn, data, length, NETCONN_COPY) != ERR_OK)
        return -1;

    return length;
}

EXPORT
void tcp_conn_local_addr(tcp_conn_t *conn, uint8_t addr[4], uint16_t *port) {
    addr[0] = ip4_addr_get_byte(&conn->local, 0);
    addr[1] = ip4_addr_get_byte(&conn->local, 1);
    addr[2] = ip4_addr_get_byte(&conn->local, 2);
    addr[3] = ip4_addr_get_byte(&conn->local, 3);

    *port = conn->local_port;
}

EXPORT
void tcp_conn_remote_addr(tcp_conn_t *conn, uint8_t addr[4], uint16_t *port) {
    addr[0] = ip4_addr_get_byte(&conn->remote, 0);
    addr[1] = ip4_addr_get_byte(&conn->remote, 1);
    addr[2] = ip4_addr_get_byte(&conn->remote, 2);
    addr[3] = ip4_addr_get_byte(&conn->remote, 3);

    *port = conn->remote_port;
}

EXPORT
void tcp_conn_close(tcp_conn_t *conn) {
    netconn_close(conn->conn);
    netconn_prepare_delete(conn->conn);
}

EXPORT
void tcp_conn_free(tcp_conn_t *conn) {
    if (conn->pending != NULL)
        netbuf_free(conn->pending);

    netconn_delete(conn->conn);

    free(conn);
}
