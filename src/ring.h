/**
 * @author Sean Hobeck
 * @date 2026-02-27
 */
#ifndef LZD_RING_H
#define LZD_RING_H

/*! @uses size_t. */
#include <stddef.h>

/*! @uses ssize_t. */
#include <sys/types.h>

/* ... */
typedef struct {
    void **data; /* array of pointers to items. */
    size_t capacity; /* capacity of the ring queue. */
    size_t head; /* pop index */
    size_t tail; /* push index */
    size_t count; /* number of items */
} ring_t;

/**
 * @brief initialize a new ring queue with capacity 16.
 *
 * @return a new ring queue with capacity 16.
 */
ring_t*
ring_init();

/** @brief free a ring as well as all of its data. */
void
ring_free(ring_t* ring_queue);

/**
 * @brief grows a ring to double of its original capacity.
 *
 * @param ring_queue the ring queue to be grown.
 * @return -1 if a failure occurs, 0 o.w.
 */
ssize_t
ring_grow(ring_t* ring_queue);

/**
 * @brief push an item into a given ring.
 *
 * @param ring_queue the ring queue to push an item to.
 * @param item a pointer to the item to be pushed.
 * @return -1 if a failure occurs, 0 o.w.
 */
ssize_t
ring_push(ring_t* ring_queue, void* item);

/**
 * @brief pop an item from the head of a given ring.
 *
 * @param ring_queue the ring queue to pop an item from.
 * @return 0x0 if empty, otherwise the end of the head of the ring queue.
 */
void*
ring_pop(ring_t* ring_queue);
#endif /* LZD_RING_H */