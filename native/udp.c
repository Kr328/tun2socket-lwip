#include "udp.h"

#include "interface.h"
#include "utils.h"

#include "lwip/udp.h"
#include "lwip/tcpip.h"
#include "lwip/ip.h"
#include "lwip/sys.h"

#include <string.h>

struct udp_conn_t {
  struct udp_pcb *pcb;

  sys_mbox_t rx;
};

static void udp_on_received(void *arg, struct udp_pcb *pcb, struct pbuf *p,
                            const ip_addr_t *src_addr, u16_t src_port) {
  udp_conn_t *conn = arg;

  uint8_t *pkt = malloc(sizeof(endpoint_t) + sizeof(struct pbuf *));

  ip_addr_t *dst_addr = ip_current_dest_addr();
  uint16_t dst_port = udp_current_dst();

  endpoint_t *header = (endpoint_t *) pkt;

  header->src_addr[0] = ip4_addr_get_byte(src_addr, 0);
  header->src_addr[1] = ip4_addr_get_byte(src_addr, 1);
  header->src_addr[2] = ip4_addr_get_byte(src_addr, 2);
  header->src_addr[3] = ip4_addr_get_byte(src_addr, 3);

  header->src_port = src_port;

  header->dst_addr[0] = ip4_addr_get_byte(dst_addr, 0);
  header->dst_addr[1] = ip4_addr_get_byte(dst_addr, 1);
  header->dst_addr[2] = ip4_addr_get_byte(dst_addr, 2);
  header->dst_addr[3] = ip4_addr_get_byte(dst_addr, 3);

  header->dst_port = dst_port;

  *((struct pbuf **) ((pkt) + sizeof(endpoint_t))) = p;

  if (sys_mbox_trypost(&conn->rx, pkt)) {
    pbuf_free(p);

    free(pkt);
  }
}

static void udp_write_into(void *ctx) {
  struct pbuf *buf = (struct pbuf *) ctx;

  udp_conn_t *conn = *(udp_conn_t **) (((uint8_t *) buf->payload) -
                                       sizeof(udp_conn_t *));
  endpoint_t *metadata = (endpoint_t *) (((uint8_t *) buf->payload) -
                                         sizeof(udp_conn_t *) - sizeof(endpoint_t));

  if (conn->pcb == NULL) {
    pbuf_free(buf);

    return;
  }

  buf->len -= sizeof(udp_conn_t *) + sizeof(endpoint_t);
  buf->tot_len -= sizeof(udp_conn_t *) + sizeof(endpoint_t);

  ip_addr_t src_addr;
  ip_addr_t dst_addr;

  IP4_ADDR(&src_addr, metadata->src_addr[0], metadata->src_addr[1], metadata->src_addr[2], metadata->src_addr[3]);
  IP4_ADDR(&dst_addr, metadata->dst_addr[0], metadata->dst_addr[1], metadata->dst_addr[2], metadata->dst_addr[3]);

  uint16_t src_port = metadata->src_port;
  uint16_t dst_port = metadata->dst_port;

  udp_sendto_if_src_port(conn->pcb, buf, &dst_addr, dst_port,
                         global_interface_get(),
                         &src_addr, src_port);

  pbuf_free(buf);
}

EXPORT
udp_conn_t *udp_conn_listen() {
  WITH_LWIP_LOCKED();

  struct udp_pcb *pcb = udp_new();
  udp_conn_t *conn = NULL;

  if (udp_bind(pcb, IP4_ADDR_ANY, UDP_ACCEPT_ANY_PORT) != ERR_OK)
    goto abort;

  conn = malloc(sizeof(udp_conn_t));

  memset(conn, 0, sizeof(udp_conn_t));

  sys_mbox_new(&conn->rx, TCPIP_MBOX_SIZE);

  udp_bind_netif(pcb, global_interface_get());

  udp_recv(pcb, &udp_on_received, conn);

  conn->pcb = pcb;

  return conn;

  abort:

  udp_remove(pcb);
  free(conn);

  return NULL;
}

EXPORT
void udp_conn_close(udp_conn_t *conn) {
  struct udp_pcb *pcb;

  {
    WITH_LWIP_LOCKED();

    pcb = conn->pcb;

    conn->pcb = NULL;
  }

  sys_mbox_post(&conn->rx, NULL);

  if (pcb != NULL) {
    tcpip_callback((void (*)(void *)) &udp_remove, pcb);
  }
}

EXPORT
void udp_conn_free(udp_conn_t *udp) {
  udp_conn_close(udp);

  udp_conn_recv(udp, NULL, NULL, 0);

  sys_mbox_free(&udp->rx);

  tcpip_callback(free, udp);
}

EXPORT
int udp_conn_recv(udp_conn_t *conn, endpoint_t *endpoint, void *buffer, int size) {
  while (conn->pcb != NULL) {
    uint8_t *pkt = NULL;

    sys_mbox_fetch(&conn->rx, (void **) &pkt);

    if (pkt == NULL) {
      continue;
    }

    memcpy(endpoint, (endpoint_t *) pkt, sizeof(endpoint_t));

    struct pbuf *pbuf = *(struct pbuf **) (pkt + sizeof(endpoint_t));

    free(pkt);

    int result = 0;

    if (size >= pbuf->tot_len) {
      pbuf_copy_partial(pbuf, buffer, pbuf->tot_len, 0);

      result = pbuf->tot_len;
    }

    pbuf_free(pbuf);

    if (result > 0)
      return result;
  }

  while (1) {
    uint8_t *pkt = NULL;

    if (sys_mbox_tryfetch(&conn->rx, (void **) &pkt)) {
      break;
    }

    struct pbuf *pbuf = *(struct pbuf **) (((uint8_t *) pkt) + sizeof(endpoint_t));

    pbuf_free(pbuf);
    free(pkt);
  }

  return -1;
}

EXPORT
int udp_conn_sendto(udp_conn_t *conn, endpoint_t *endpoint, void *buffer, int size) {
  if (!conn->pcb)
    return -1;

  struct pbuf *buf = pbuf_alloc(PBUF_TRANSPORT, size + sizeof(endpoint_t) + sizeof(udp_conn_t *), PBUF_RAM);
  if (buf->next != NULL) {
    pbuf_free(buf);

    return 0;
  }

  pbuf_take_at(buf, buffer, size, 0);
  pbuf_take_at(buf, endpoint, sizeof(endpoint_t), size);
  pbuf_take_at(buf, &conn, sizeof(udp_conn_t *), size + sizeof(endpoint_t));

  if (conn->pcb == NULL || tcpip_try_callback(&udp_write_into, buf) != ERR_OK) {
    pbuf_free(buf);
  }

  return size;
}
