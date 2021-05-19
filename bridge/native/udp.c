#include "udp.h"

#include "queues.h"
#include "interface.h"
#include "utils.h"

#include "lwip/udp.h"
#include "lwip/tcpip.h"
#include "lwip/ip.h"

#include <threads.h>
#include <string.h>

struct udp_conn_t {
    struct udp_pcb *pcb;

    pbuf_queue_t rx;
    pbuf_queue_t tx;

    mtx_t rx_lock;
    cnd_t rx_cond;

    mtx_t tx_lock;
    int tx_polling;
};

static void udp_on_received(void *arg, struct udp_pcb *pcb, struct pbuf *p,
                            const ip_addr_t *src_addr, u16_t src_port) {
    udp_conn_t *conn = arg;

    struct pbuf *buffer = pbuf_alloc(PBUF_RAW, sizeof(struct udp_metadata_t), PBUF_RAM);
    if (buffer == NULL)
        return;

    ip_addr_t *dst_addr = ip_current_dest_addr();
    uint16_t dst_port = udp_current_dst();

    struct udp_metadata_t header;

    header.src_addr[0] = ip4_addr_get_byte(src_addr, 0);
    header.src_addr[1] = ip4_addr_get_byte(src_addr, 1);
    header.src_addr[2] = ip4_addr_get_byte(src_addr, 2);
    header.src_addr[3] = ip4_addr_get_byte(src_addr, 3);

    header.src_port = src_port;

    header.dst_addr[0] = ip4_addr_get_byte(dst_addr, 0);
    header.dst_addr[1] = ip4_addr_get_byte(dst_addr, 1);
    header.dst_addr[2] = ip4_addr_get_byte(dst_addr, 2);
    header.dst_addr[3] = ip4_addr_get_byte(dst_addr, 3);

    header.dst_port = dst_port;

    pbuf_take(buffer, &header, sizeof(header));

    pbuf_cat(buffer, p);

    WITH_MUTEX_LOCKED(lock, &conn->rx_lock);

    pbuf_queue_append(&conn->rx, &buffer, 1);

    cnd_signal(&conn->rx_cond);
}

static void udp_poll_tx(void *ctx) {
    udp_conn_t *conn = ctx;

    struct pbuf *array[32];
    int size;

    {
        WITH_MUTEX_LOCKED(lock, &conn->tx_lock);

        conn->tx_polling = 0;

        size = pbuf_queue_pop(&conn->tx, array, 32);
    }

    for (int i = 0; i < size; i++) {
        struct pbuf *buf = array[i];

        udp_metadata_t *metadata = (udp_metadata_t*) buf->payload;

        ip_addr_t src_addr;
        ip_addr_t dst_addr;

        IP4_ADDR(&src_addr, metadata->src_addr[0], metadata->src_addr[1], metadata->src_addr[2], metadata->src_addr[3]);
        IP4_ADDR(&dst_addr, metadata->dst_addr[0], metadata->dst_addr[1], metadata->dst_addr[2], metadata->dst_addr[3]);

        uint16_t src_port = metadata->src_port;
        uint16_t dst_port = metadata->dst_port;

        if (pbuf_remove_header(buf, sizeof(udp_metadata_t))) {
            pbuf_free(buf);

            continue;
        }

        udp_sendto_if_src_port_chksum(conn->pcb, buf, &dst_addr, dst_port,
                                                  global_interface_get(),
                                                  0, 0,
                                                  &src_addr, src_port);

        pbuf_free(buf);
    }
}

udp_conn_t *udp_conn_listen() {
    LOCK_TCPIP_CORE();

    struct udp_pcb *pcb = udp_new();
    struct udp_conn_t *conn = NULL;

    if (udp_bind(pcb, IP4_ADDR_ANY, UDP_ACCEPT_ANY_PORT) != ERR_OK)
        goto abort;

    conn = malloc(sizeof(udp_conn_t));

    memset(conn, 0, sizeof(udp_conn_t));

    mtx_init(&conn->rx_lock, mtx_plain);
    mtx_init(&conn->tx_lock, mtx_plain);

    cnd_init(&conn->rx_cond);

    udp_bind_netif(pcb, global_interface_get());

    udp_recv(pcb, &udp_on_received, conn);

    conn->pcb = pcb;

    UNLOCK_TCPIP_CORE();

    return conn;

    abort:

    udp_remove(pcb);
    free(conn);

    UNLOCK_TCPIP_CORE();

    return NULL;
}

void udp_conn_close(udp_conn_t *conn) {
    LOCK_TCPIP_CORE();

    WITH_MUTEX_LOCKED(rx_lock, &conn->rx_lock);
    WITH_MUTEX_LOCKED(tx_lock, &conn->tx_lock);

    if (conn->pcb != NULL)
        udp_remove(conn->pcb);

    conn->pcb = NULL;

    cnd_broadcast(&conn->rx_cond);

    UNLOCK_TCPIP_CORE();
}

void udp_conn_free(udp_conn_t *udp) {
    udp_conn_close(udp);

    free(udp);
}

int udp_conn_recv(udp_conn_t *conn, udp_metadata_t *metadata, void *buffer, int size) {
    struct pbuf *buf = NULL;

    {
        WITH_MUTEX_LOCKED(lock, &conn->rx_lock);

        while (pbuf_queue_length(&conn->rx) == 0) {
            if (conn->pcb == NULL)
                return -1;

            cnd_wait(&conn->rx_cond, &conn->rx_lock);
        }

        pbuf_queue_pop(&conn->rx, &buf, 1);
    }

    if (buf == NULL)
        return -1;

    if (buf->tot_len - sizeof(udp_metadata_t) > size) {
        pbuf_free(buf);

        return -1;
    }

    unsigned data_length = buf->tot_len - sizeof(udp_metadata_t);

    pbuf_copy_partial(buf, metadata, sizeof(udp_metadata_t), 0);
    pbuf_copy_partial(buf, buffer, data_length, sizeof(udp_metadata_t));

    pbuf_free(buf);

    return (int) data_length;
}

int udp_conn_sendto(udp_conn_t *conn, udp_metadata_t *metadata, void *buffer, int size) {
    struct pbuf *buf = pbuf_alloc(PBUF_TRANSPORT, size, PBUF_RAM);
    if (buf == NULL)
        return -1;

    pbuf_take(buf, buffer, size);

    if (pbuf_add_header(buf, sizeof(udp_metadata_t))) {
        pbuf_free(buf);

        return -1;
    }

    pbuf_take(buf, metadata, sizeof(udp_metadata_t));

    WITH_MUTEX_LOCKED(lock, &conn->tx_lock);

    pbuf_queue_append(&conn->tx, &buf, 1);

    if (!conn->tx_polling) {
        if (tcpip_try_callback(&udp_poll_tx, conn) == ERR_OK) {
            conn->tx_polling = 1;
        }
    }

    return size;
}
