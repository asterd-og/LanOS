#pragma once

#define NEW_LOCK(x) int x = 0

void spinlock_lock(int *lock);
void spinlock_free(int *lock);
