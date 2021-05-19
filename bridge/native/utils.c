#include "utils.h"

void scoped_mutex_acquire(mtx_t *mutex) {
    mtx_lock(mutex);
}

void scoped_mutex_release(mtx_t **mutex) {
    mtx_unlock(*mutex);
}
