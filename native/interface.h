#pragma once

#include <stdint.h>

#include "lwip/pbuf.h"
#include "lwip/netif.h"

#define DEFAULT_MTU 1500

typedef void (*global_interface_output_func)(void *ctx, struct pbuf *p);

void global_interface_init();

void global_interface_attach_device(global_interface_output_func output, void *state, int mtu);

int global_interface_is_attached();

struct netif *global_interface_get();