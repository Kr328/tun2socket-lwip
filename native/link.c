#include "link.h"

#include "utils.h"
#include "interface.h"

#include "lwip/tcpip.h"
#include "lwip/sys.h"

#include <string.h>
#include <stdlib.h>
#include <stdatomic.h>

struct link_t {
  sys_mbox_t rx;

  int closed;
  int mtu;
};

static void if_output(void *context, struct pbuf *p) {
  link_t *ctx = (link_t *) context;

  if (ctx->closed || sys_mbox_trypost(&ctx->rx, p) != ERR_OK) {
    pbuf_free(p);
  }
}

EXPORT
link_t *link_attach(int mtu) {
  WITH_LWIP_LOCKED();

  if (global_interface_is_attached())
    return NULL;

  link_t *ctx = (link_t *) malloc(sizeof(link_t));

  memset(ctx, 0, sizeof(link_t));

  sys_mbox_new(&ctx->rx, TCPIP_MBOX_SIZE);

  ctx->closed = 0;
  ctx->mtu = mtu;

  global_interface_attach_device(&if_output, ctx, ctx->mtu);

  return ctx;
}

EXPORT
void link_close(link_t *ctx) {
  {
    WITH_LWIP_LOCKED();

    if (!ctx->closed) {
      global_interface_attach_device(NULL, NULL, DEFAULT_MTU);
    }

    ctx->closed = 1;
  }

  sys_mbox_post(&ctx->rx, NULL);
}

EXPORT
void link_free(link_t *ctx) {
  link_close(ctx);

  link_read(ctx, NULL, 0);

  sys_mbox_free(&ctx->rx);

  free(ctx);
}

EXPORT
int link_read(link_t *ctx, void *buffer, int size) {
  while (!ctx->closed) {
    struct pbuf *source = NULL;

    sys_mbox_fetch(&ctx->rx, (void **) &source);
    if (source == NULL) {
      continue;
    }

    int result = 0;

    if (size >= source->tot_len) {
      pbuf_copy_partial(source, buffer, source->tot_len, 0);

      result = source->tot_len;
    }

    pbuf_free(source);

    if (result > 0)
      return size;
  }

  while (1) {
    struct pbuf *msg = NULL;

    if (sys_mbox_tryfetch(&ctx->rx, (void **) &msg)) {
      break;
    }

    if (msg != NULL) {
      pbuf_free(msg);
    }
  }

  return -1;
}

EXPORT
int link_write(link_t *ctx, void *buffer, int size) {
  if (ctx->closed)
    return -1;

  struct pbuf *target = pbuf_alloc(PBUF_IP, size, PBUF_POOL);

  pbuf_take(target, buffer, size);

  if (tcpip_inpkt(target, global_interface_get(), global_interface_get()->input) != ERR_OK) {
    pbuf_free(target);
  }

  return size;
}

