#include "utils.h"

#include "lwip/tcpip.h"

void scoped_mutex_acquire(pthread_mutex_t *mutex) {
    pthread_mutex_lock(mutex);
}

void scoped_mutex_release(pthread_mutex_t **mutex) {
    pthread_mutex_unlock(*mutex);
}

void scoped_lwip_lock_acquire() {
    LOCK_TCPIP_CORE();
}

void scoped_lwip_lock_release(const int *placeholder) {
    (void) placeholder;

    UNLOCK_TCPIP_CORE();
}