#include "init.h"

#include "utils.h"
#include "interface.h"

#include "lwip/tcpip.h"

static void tcpip_initialize(void *arg) {
  global_interface_init();

  sys_mutex_t *lock = arg;

  sys_mutex_unlock(lock);
}

EXPORT
void init_lwip() {
  sys_mutex_t lock;

  sys_mutex_new(&lock);

  sys_mutex_lock(&lock);

  tcpip_init(tcpip_initialize, &lock);

  sys_mutex_lock(&lock);

  sys_mutex_unlock(&lock);

  sys_mutex_free(&lock);
}
