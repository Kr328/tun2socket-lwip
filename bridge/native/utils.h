#pragma once

#include <threads.h>

#define CLEANUP(func) __attribute__((cleanup(func)))
#define UNUSED __attribute__((unused))
#define EXPORT __attribute__((visibility("default"), used))

void scoped_mutex_acquire(mtx_t *mutex);
void scoped_mutex_release(mtx_t **mutex);

#define WITH_MUTEX_LOCKED(key, mutex) CLEANUP(scoped_mutex_release) mtx_t *__locker_##key = (mutex); scoped_mutex_acquire(mutex)
