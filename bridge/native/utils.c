#include "utils.h"

void scoped_mutex_acquire(pthread_mutex_t *mutex) {
    pthread_mutex_lock(mutex);
}

void scoped_mutex_release(pthread_mutex_t **mutex) {
    pthread_mutex_unlock(*mutex);
}
