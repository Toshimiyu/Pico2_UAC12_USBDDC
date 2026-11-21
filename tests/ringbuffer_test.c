#include <assert.h>
#include <limits.h>
#include <pthread.h>
#include <sched.h>
#include <stdbool.h>
#include <stdlib.h>

static bool fail_allocation;

static void *test_malloc(size_t size)
{
    return fail_allocation ? NULL : malloc(size);
}

#define malloc test_malloc
#include "../src/ringbuffer.c"
#undef malloc

enum { SAMPLE_COUNT = 100000 };
static RINGBUFFER shared;

static void *produce(void *unused)
{
    (void)unused;
    for (int32_t i = 0; i < SAMPLE_COUNT; i++)
        while (ringbuf_write_spinlock(i, &shared) < 0)
            sched_yield();
    return NULL;
}

static void *consume(void *unused)
{
    (void)unused;
    for (int32_t i = 0; i < SAMPLE_COUNT; i++)
    {
        int32_t sample;
        while (ringbuf_read_spinlock(&sample, &shared) < 0)
            sched_yield();
        assert(sample == i);
        assert(get_size_using(&shared) >= 0);
        assert(get_size_remain(&shared) >= 0);
    }
    return NULL;
}

int main(void)
{
    RINGBUFFER rb = {0};
    fail_allocation = true;
    assert(initialize_ringbuffer(4, false, &rb) == -1);
    assert(test_lock_claims == 0);
    fail_allocation = false;
    assert(initialize_ringbuffer(0, false, &rb) == -1);
    assert(initialize_ringbuffer(UINT32_MAX, false, &rb) == -1);
    assert(test_lock_claims == 0);

    assert(initialize_ringbuffer(4, false, &rb) == 0);
    unsigned entries = atomic_load(&test_lock_entries);
    assert(get_size_using(&rb) == 0);
    assert(get_size_remain(&rb) == 4);
    assert(atomic_load(&test_lock_entries) == entries + 2);
    int32_t input[] = {1, 2, 3, 4};
    int32_t output[4];
    assert(ringbuf_read_array_spinlock(output, 1, &rb) == -1);
    assert(ringbuf_write_array_spinlock(input, 4, &rb) == 4);
    assert(ringbuffer_is_full(&rb));
    assert(get_size_remain(&rb) == 0);
    assert(ringbuf_write_spinlock(5, &rb) == -1);
    assert(ringbuf_read_array_spinlock(output, 3, &rb) == 3);
    assert(output[0] == 1 && output[1] == 2 && output[2] == 3);
    assert(ringbuf_write_array_spinlock(input, 3, &rb) == 3);
    assert(ringbuffer_is_full(&rb));
    assert(ringbuf_read_array_spinlock(output, 4, &rb) == 4);
    assert(output[0] == 4 && output[1] == 1 && output[2] == 2 && output[3] == 3);
    assert(get_size_using(&rb) == 0);
    clear_ringbuffer(&rb);
    assert(get_read_point(&rb) == 0 && get_write_point(&rb) == 0);
    free(rb.buffer);

    assert(initialize_ringbuffer(2, true, &rb) == 0);
    entries = atomic_load(&test_lock_entries);
    assert(ringbuf_write_no_spinlock(7, &rb) == 1);
    assert(get_size_using(&rb) == 1);
    assert(ringbuf_read_no_spinlock(output, &rb) == 1 && output[0] == 7);
    clear_ringbuffer(&rb);
    assert(atomic_load(&test_lock_entries) == entries);
    free(rb.buffer);

    assert(initialize_ringbuffer(64, false, &shared) == 0);
    pthread_t writer, reader;
    assert(pthread_create(&writer, NULL, produce, NULL) == 0);
    assert(pthread_create(&reader, NULL, consume, NULL) == 0);
    assert(pthread_join(writer, NULL) == 0);
    assert(pthread_join(reader, NULL) == 0);
    assert(get_size_using(&shared) == 0);
    free(shared.buffer);
    return 0;
}
