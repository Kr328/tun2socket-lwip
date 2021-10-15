#include "tcp.h"

#include "interface.h"

#include "lwip/sys.h"
#include "lwip/tcp.h"

#include <string.h>
#include <stdatomic.h>

struct tcp_conn_t {
  struct tcp_poller_t *poller;

  uint16_t index;
  struct tcp_pcb *conn;

  struct pbuf *write_buffer;
  struct pbuf *read_buffer;

  int readable;
  int writable;

  struct pbuf *pending;

  int should_notify_writable;
  int should_notify_readable;
};

struct tcp_poller_t {
  struct tcp_conn_t *conns[TCP_CONNECTION_SIZE];

  uint16_t conns_available[TCP_CONNECTION_SIZE];
  int conns_available_size;

  sys_mutex_t available_lock;

  struct tcp_pcb *listener;

  sys_mbox_t accepts;
  sys_mbox_t events;

  int closed;
};

static int find_available_index(tcp_poller_t *poller) {
  WITH_MUTEX_LOCKED(available, &poller->available_lock);

  if (poller->conns_available_size > 0) {
    return poller->conns_available[--poller->conns_available_size];
  }

  return -1;
}

static void recycle_available_index(tcp_poller_t *poller, uint16_t index) {
  WITH_MUTEX_LOCKED(available, &poller->available_lock);

  poller->conns_available[poller->conns_available_size++] = index;
}

static void send_event(tcp_poller_t *poller, uint16_t index, uint16_t event) {
  sys_mbox_post(&poller->events, (void *) ((((unsigned long)index) << 16) | event));
}

static err_t listener_accept(void *arg, struct tcp_pcb *newpcb, err_t err) {
  if (arg == NULL || newpcb == NULL) {
    return ERR_VAL;
  }

  if (err != ERR_OK) {
    tcp_close(newpcb);

    return ERR_OK;
  }

  tcp_poller_t *poller = (tcp_poller_t *) arg;

  endpoint_t *endpoint = (endpoint_t *) malloc(sizeof(endpoint_t));

  endpoint->src_addr[0] = ip4_addr_get_byte(&newpcb->remote_ip, 0);
  endpoint->src_addr[1] = ip4_addr_get_byte(&newpcb->remote_ip, 1);
  endpoint->src_addr[2] = ip4_addr_get_byte(&newpcb->remote_ip, 2);
  endpoint->src_addr[3] = ip4_addr_get_byte(&newpcb->remote_ip, 3);

  endpoint->src_port = newpcb->remote_port;

  endpoint->dst_addr[0] = ip4_addr_get_byte(&newpcb->local_ip, 0);
  endpoint->dst_addr[1] = ip4_addr_get_byte(&newpcb->local_ip, 1);
  endpoint->dst_addr[2] = ip4_addr_get_byte(&newpcb->local_ip, 2);
  endpoint->dst_addr[3] = ip4_addr_get_byte(&newpcb->local_ip, 3);

  endpoint->dst_port = newpcb->local_port;

  tcp_arg(newpcb, endpoint);

  tcp_backlog_delayed(newpcb);

  if (sys_mbox_trypost(&poller->accepts, newpcb) != ERR_OK) {
    free(endpoint);
    tcp_close(newpcb);
  }

  return ERR_OK;
}

static void listener_error(void *arg, err_t err) {
  tcp_poller_t *poller = (tcp_poller_t *) arg;

  if (err != ERR_OK) {
    poller->listener = NULL;

    sys_mbox_trypost(&poller->accepts, NULL);
  }
}

static err_t conn_received(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
  struct tcp_conn_t *conn = arg;

  if (p == NULL || err != ERR_OK) {
    conn->readable = 0;
  } else {
    if (conn->read_buffer != NULL) {
      pbuf_cat(conn->read_buffer, p);
    } else {
      conn->read_buffer = p;
    }
  }

  if (conn->should_notify_readable) {
    send_event(conn->poller, conn->index, EVENT_READABLE);

    conn->should_notify_readable = 0;
  }

  return ERR_OK;
}

static err_t conn_sent(void *arg, struct tcp_pcb *tpcb, u16_t len) {
  struct tcp_conn_t *conn = arg;

  pbuf_free_header(conn->write_buffer, len);

  if (conn->should_notify_writable) {
    send_event(conn->poller, conn->index, EVENT_WRITABLE);

    conn->should_notify_writable = 0;
  }

  return ERR_OK;
}

static void conn_err(void *arg, err_t err) {
  struct tcp_conn_t *conn = arg;

  conn->conn = NULL;

  conn->readable = 0;
  conn->writable = 0;

  send_event(conn->poller, conn->index, EVENT_READABLE);
  send_event(conn->poller, conn->index, EVENT_WRITABLE);
}

tcp_poller_t *new_tcp_poller() {
  WITH_LWIP_LOCKED();

  struct tcp_pcb *listener = tcp_new();
  tcp_poller_t *result = (tcp_poller_t *) malloc(sizeof(tcp_poller_t));

  tcp_arg(listener, result);
  tcp_accept(listener, &listener_accept);
  tcp_err(listener, &listener_error);

  tcp_bind_netif(listener, global_interface_get());

  if (tcp_bind(listener, IP_ADDR_ANY, TCP_ACCEPT_ANY_PORT) != ERR_OK) {
    goto abort;
  }

  if (tcp_listen_with_backlog(listener, TCP_LISTEN_BACKLOG)) {
    goto abort;
  }

  return result;

  abort:
  tcp_close(listener);
  free(result);

  return NULL;
}

void tcp_poller_close(tcp_poller_t *poller) {
  WITH_LWIP_LOCKED();

  if (poller->listener != NULL) {
    tcp_accept(poller->listener, NULL);
    tcp_err(poller->listener, NULL);
    tcp_close(poller->listener);
  }

  listener_error(poller, ERR_CLSD);

  // TODO: purge conns
}

void tcp_poller_free(tcp_poller_t *poller) {
  tcp_poller_close(poller);

  tcp_poller_accept(poller, NULL, NULL);

  sys_mbox_free(&poller->accepts);
  sys_mbox_free(&poller->events);

  free(poller);
}

int tcp_poller_accept(tcp_poller_t *poller, uint16_t *index, endpoint_t *endpoint) {
  while (poller->listener != NULL) {
    struct tcp_pcb *newpcb = NULL;

    sys_mbox_fetch(&poller->accepts, (void **) &newpcb);

    if (newpcb == NULL) {
      continue;
    }

    int idx = find_available_index(poller);
    if (idx < 0) {
      WITH_LWIP_LOCKED();

      free(newpcb->callback_arg);

      tcp_close(newpcb);

      continue;
    }

    memcpy(endpoint, newpcb->callback_arg, sizeof(endpoint_t));

    free(newpcb->callback_arg);

    *index = (uint16_t) idx;

    WITH_LWIP_LOCKED();

    tcp_recv(newpcb, &conn_received);
    tcp_sent(newpcb, &conn_sent);
    tcp_err(newpcb, &conn_err);

    return 0;
  }

  while (1) {
    struct tcp_pcb *newpcb = NULL;

    if (sys_mbox_tryfetch(&poller->accepts, (void **) &newpcb)) {
      break;
    }

    free(newpcb->callback_arg);

    {
      WITH_LWIP_LOCKED();

      tcp_close(newpcb);
    }
  }

  return -1;
}

int tcp_poller_events(tcp_poller_t *poller, uint32_t *events, int size) {
  int count = 0;

  while (!poller->closed && size > 0) {
    unsigned long event = 0;

    if (count == 0) {
      sys_mbox_fetch(&poller->events, (void **) &event);
    } else {
      if (sys_mbox_tryfetch(&poller->events, (void **) &event)) {
        break;
      }
    }
    if ((event & 0xFFFF) == 0) {
      continue;
    }

    *events++ = event;
    count++;
    size--;
  }

  return count == 0 ? -1 : 0;
}

int tcp_poller_read_index(tcp_poller_t *poller, uint16_t index, void *buffer, int size) {
  struct tcp_conn_t *conn = poller->conns[index];

  if (!conn->readable) {
    return -1;
  }

  struct pbuf *read_buffer = NULL;
  {
    WITH_LWIP_LOCKED();

    read_buffer = conn->read_buffer;
  }

  if (read_buffer == NULL || read_buffer->tot_len == 0) {
    if (read_buffer != NULL) {
      pbuf_free(read_buffer);
    }

    WITH_LWIP_LOCKED();

    conn->should_notify_readable = 1;

    return 0;
  }

  if (read_buffer->tot_len < size) {
    size = read_buffer->tot_len;
  }

  pbuf_copy_partial(read_buffer, buffer, size, 0);

  read_buffer = pbuf_free_header(read_buffer, size);

  WITH_LWIP_LOCKED();

  if (conn->read_buffer != NULL) {
    pbuf_cat(read_buffer, conn->read_buffer);
  }

  conn->read_buffer = read_buffer;

  if (conn->conn != NULL) {
    tcp_recved(conn->conn, size);
  }

  return size;
}

int tcp_poller_write_index(tcp_poller_t *poller, uint16_t index, const void *buffer, int size) {
  struct tcp_conn_t *conn = poller->conns[index];

  if (!conn->writable) {
    return -1;
  }

  if (conn->pending != NULL) {
    WITH_LWIP_LOCKED();

    LWIP_ASSERT("Writing buffer should not be a chain", conn->pending->next == NULL);

    if (conn->conn == NULL || tcp_write(conn->conn, conn->pending->payload, conn->pending->len, 0) != ERR_OK) {
      conn->should_notify_writable = 1;

      return 0;
    }

    pbuf_cat(conn->write_buffer, conn->pending);

    conn->pending = NULL;
  }

  if (buffer == NULL) {
    return -1;
  }

  struct pbuf *buf = pbuf_alloc(PBUF_TRANSPORT, size, PBUF_RAM);

  pbuf_take(buf, buffer, size);

  conn->pending = buf;

  tcp_poller_write_index(poller, index, NULL, 0);

  return size;
}

int tcp_poller_close_index(tcp_poller_t *poller, uint16_t index) {
  return 0;
}

