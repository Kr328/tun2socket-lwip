#include "interface.h"

#include "lwip/netif.h"
#include "lwip/ip.h"

#include <assert.h>
#include <stdio.h>

static struct netif global_if;

static global_interface_output_func output_func;
static void *output_context;

static err_t global_if_output(struct netif *netif, struct pbuf *p, const ip4_addr_t *ipaddr) {
    if (output_func != NULL) {
        pbuf_ref(p);

        output_func(output_context, p);
    }

    return ERR_OK;
}

static err_t global_if_init(struct netif *n) {
    n->name[0] = 'e';
    n->name[1] = 'n';

    n->output = &global_if_output;
    n->state = NULL;

    return ERR_OK;
}

void global_interface_init() {
    struct netif *created = netif_add(&global_if, IP4_ADDR_ANY, IP4_ADDR_ANY, IP4_ADDR_ANY, NULL, &global_if_init, ip_input);

    assert(created != NULL);

    created->mtu = DEFAULT_MTU;

    netif_set_up(created);
    netif_set_link_up(created);
    netif_set_default(created);
}

void global_interface_inject_packet(struct pbuf *buf) {
    if (buf == NULL)
        return;

    if (global_if.input(buf, &global_if) != ERR_OK) {
        pbuf_free(buf);
    }
}

void global_interface_attach_device(global_interface_output_func output, void *state, int mtu) {
    LWIP_ASSERT_CORE_LOCKED();

    output_func = output;
    output_context = state;

    if (mtu <= 0)
        mtu = DEFAULT_MTU;

    global_if.mtu = mtu;
}

int global_interface_is_attached() {
    LWIP_ASSERT_CORE_LOCKED();

    return output_func != NULL || output_context != NULL;
}

struct netif *global_interface_get() {
    return &global_if;
}
