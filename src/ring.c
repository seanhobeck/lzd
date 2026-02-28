/**
 * @author Sean Hobeck
 * @date 2026-02-27
 */
#include "ring.h"

/*! @uses fprintf, stderr. */
#include <stdio.h>

/*! @uses calloc, free. */
#include <stdlib.h>

/**
 * @brief initialize a new ring queue with capacity 16.
 *
 * @return a new ring queue with capacity 16.
 */
ring_t*
ring_init() {
    /* allocate more memory and free. */
    ring_t* ring_queue = calloc(1u, sizeof *ring_queue);
    ring_queue->data = (void**) calloc(16u, sizeof(void *));
    if (!ring_queue->data) return 0x0;
    ring_queue->capacity = 16u;
    ring_queue->head = 0x0;
    ring_queue->tail = 0x0;
    ring_queue->count = 0u;
    return ring_queue;
}

/** @brief free a ring as well as all of its data. */
void
ring_free(ring_t* ring_queue) {
    if (!ring_queue) return;
    free(ring_queue->data);
    ring_queue->data = NULL;
    ring_queue->capacity = 0;
    ring_queue->head = 0;
    ring_queue->tail = 0;
    ring_queue->count = 0;
}

/**
 * @brief grows a ring to double of its original capacity.
 *
 * @param ring_queue the ring queue to be grown.
 * @return -1 if a failure occurs, 0 o.w.
 */
ssize_t
ring_grow(ring_t* ring_queue) {
    size_t mew_capacity = (ring_queue->capacity == 0u) ? 16u : (ring_queue->capacity * 2u);

    /* allocate for the newcap. */
    void** tmp = calloc(mew_capacity, sizeof(void*));
    if (!tmp) {
        fprintf(stderr, "tapi, ring_grow; calloc failed; could not allocate memory for grow.");
        return -1;
    }

    /* linearize into [0..count) */
    for (size_t i = 0; i < ring_queue->count; i++) {
        size_t idx = (ring_queue->head + i) % ring_queue->capacity;
        tmp[i] = ring_queue->data[idx];
    }
    free(ring_queue->data);
    ring_queue->data = tmp;
    ring_queue->capacity = mew_capacity;
    ring_queue->head = 0x0;
    ring_queue->tail = ring_queue->count;
    return 0u;
}

/**
 * @brief push an item into a given ring.
 *
 * @param ring_queue the ring queue to push an item to.
 * @param item a pointer to the item to be pushed.
 * @return -1 if a failure occurs, 0 o.w.
 */
ssize_t
ring_push(ring_t* ring_queue, void* item) {
    if (ring_queue->count == ring_queue->capacity) {
        if (ring_grow(ring_queue) != 0u) {
            fprintf(stderr, "tapi, ring_push; grow failed; could not grow ring.");
            return -1;
        }
    }

    /* push the item into the ring. */
    ring_queue->data[ring_queue->tail] = item;
    ring_queue->tail = (ring_queue->tail + 1u) % ring_queue->capacity;
    ring_queue->count++;
    return 0u;
}

/**
 * @brief pop an item from the head of a given ring.
 *
 * @param ring_queue the ring queue to pop an item from.
 * @return 0x0 if empty, otherwise the end of the head of the ring queue.
 */
void*
ring_pop(ring_t* ring_queue) {
    /* return 0x0 on empty. */
    if (ring_queue->count == 0u) return 0x0;

    /* O(1). */
    void *item = ring_queue->data[ring_queue->head];
    ring_queue->data[ring_queue->head] = 0x0;
    ring_queue->head = (ring_queue->head + 1u) % ring_queue->capacity;
    ring_queue->count--;
    return item;
}