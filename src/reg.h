/**
 * @author Sean Hobeck
 * @date 2026-02-28
 */
#ifndef LZD_REG_H
#define LZD_REG_H

/*! @uses pid_t. */
#include <sched.h>

/*! @uses uint64_t. */
#include <stdint.h>

/*! @uses ssize_t. */
#include <sys/types.h>

/* ... */
typedef struct {
    pid_t pid; /* target process id. */
    uint64_t base; /* pointer to the base of the region. */
    size_t size; /* number of bytes read, should be 0x1000 or 4KiB. */
    struct {
        size_t size, count;
    } page; /* page details. */
    unsigned char* data, *present; /* data buffer, 1 byte per page, 1=readable, 0=hole. */
} region_t;

/**
 * @brief initialize a region of memory to be read from a processes' memory; [start, end).
 *
 * @param pid the target process id.
 * @param start the start of the region.
 * @param end the end of the region.
 * @return a pointer to the allocated region if successful, 0x0 o.w.
 */
region_t*
region_init(pid_t pid, uint64_t start, uint64_t end);

/**
 * @brief free a region as well as all of its data.
 *
 * @param region the region to be freed.
 */
void
region_free(region_t* region);

/**
 * @brief read pages into the regions data, marking the present bit per page.
 *
 * @param region the region to be read.
 * @return a count of the number of pages if successful, -1 o.w.
 */
ssize_t
region_read(region_t* region);
#endif /* LZD_REG_H */