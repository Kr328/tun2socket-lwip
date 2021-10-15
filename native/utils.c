#include "utils.h"

#include "lwip/tcpip.h"
#include "lwip/sys.h"

void scoped_mutex_acquire(sys_mutex_t *mutex) {
  sys_mutex_lock(mutex);
}

void scoped_mutex_release(sys_mutex_t **mutex) {
  sys_mutex_unlock(*mutex);
}

void scoped_lwip_lock_acquire() {
  LOCK_TCPIP_CORE();
}

void scoped_lwip_lock_release(const int *placeholder) {
  (void) placeholder;

  UNLOCK_TCPIP_CORE();
}