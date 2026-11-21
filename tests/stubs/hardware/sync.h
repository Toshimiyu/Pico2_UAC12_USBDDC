#ifndef TEST_HARDWARE_SYNC_H
#define TEST_HARDWARE_SYNC_H

#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>

typedef pthread_mutex_t spin_lock_t;

static spin_lock_t test_spinlock = PTHREAD_MUTEX_INITIALIZER;
static unsigned test_lock_claims;
static atomic_uint test_lock_entries;

static inline unsigned spin_lock_claim_unused(bool required)
{
    assert(required);
    test_lock_claims++;
    return 0;
}

static inline spin_lock_t *spin_lock_init(unsigned number)
{
    assert(number == 0);
    return &test_spinlock;
}

static inline uint32_t spin_lock_blocking(spin_lock_t *lock)
{
    assert(pthread_mutex_lock(lock) == 0);
    atomic_fetch_add_explicit(&test_lock_entries, 1, memory_order_relaxed);
    return 0;
}

static inline void spin_unlock(spin_lock_t *lock, uint32_t owner)
{
    (void)owner;
    assert(pthread_mutex_unlock(lock) == 0);
}

#endif
