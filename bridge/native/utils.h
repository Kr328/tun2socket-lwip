#pragma once

#include <pthread.h>

#define CLEANUP(func) __attribute__((cleanup(func)))
#define EXPORT __attribute__((visibility("default"), used))

void scoped_mutex_acquire(pthread_mutex_t *mutex);
void scoped_mutex_release(pthread_mutex_t **mutex);

#define WITH_MUTEX_LOCKED(key, mutex) CLEANUP(scoped_mutex_release) pthread_mutex_t *__locker_##key = (mutex); scoped_mutex_acquire(mutex)

void scoped_lwip_lock_acquire();
void scoped_lwip_lock_release(const int *placeholder);

#define WITH_LWIP_LOCKED() CLEANUP(scoped_lwip_lock_release) int __lwip_core_locker; scoped_lwip_lock_acquire()