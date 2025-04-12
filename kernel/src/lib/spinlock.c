#include <spinlock.h>

void spinlock_lock(int *lock) {
    while (__sync_lock_test_and_set(lock, 1))
        while (*lock == 1)
            __asm__ volatile ("pause");
}

void spinlock_free(int *lock) {
    __sync_lock_release(lock);
}
