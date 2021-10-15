#pragma once

#include <stdint.h>

#define CLEANUP(func) __attribute__((cleanup(func)))
#define EXPORT __attribute__((visibility("default"), used))

#define WITH_MUTEX_LOCKED(key, mutex) CLEANUP(scoped_mutex_release) sys_mutex_t *__locker_##key = (mutex); scoped_mutex_acquire(mutex)
#define WITH_LWIP_LOCKED() CLEANUP(scoped_lwip_lock_release) int __lwip_core_locker; scoped_lwip_lock_acquire()

typedef struct sys_mutex * sys_mutex_t;

typedef struct endpoint_t {
  uint8_t src_addr[4];
  uint8_t dst_addr[4];
  uint16_t src_port;
  uint16_t dst_port;
} endpoint_t;

void scoped_mutex_acquire(sys_mutex_t *mutex);

void scoped_mutex_release(sys_mutex_t **mutex);

void scoped_lwip_lock_acquire();

void scoped_lwip_lock_release(const int *placeholder);