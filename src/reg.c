/**
 * @author Sean Hobeck
 * @date 2026-02-28
 */
#include "reg.h"

/*! @uses errno. */
#include <errno.h>

/*! @uses process_vm_readv. */
#include <sys/uio.h>

/*! @uses pread, close. */
#include <unistd.h>

/*! @uses open. */
#include <fcntl.h>

/*! @uses fprintf, stderr. */
#include <stdio.h>

/*! @uses calloc, free, malloc. */
#include <stdlib.h>

/*! @uses snprintf. */
#include <string.h>

/*! @uses internal. */
#include "dyna.h"

/* macros for aligning a page up and down. */
#define align_down(x, a) ((x) & ~(a - 1))
#define align_up(x, a) (((x) + (a - 1)) & ~(a - 1))

/**
 * @brief read a processes virtual memory into a buffer.
 *
 * @param pid the target process id.
 * @param address the address of memory to start reading.
 * @param destination a pointer to the destination buffer.
 * @param length the length of the destination buffer.
 * @return number of bytes read if successful, 0 o.w. rtfm.
 */
internal ssize_t
read_rempr(pid_t pid, uint64_t address, void* destination, size_t length) {
    struct iovec local = { .iov_base = destination, .iov_len = length };
    struct iovec remote = { .iov_base = (void*)address, .iov_len = length };
    ssize_t read = process_vm_readv(pid, &local, 1, &remote, 1, 0);

    /* if we can read through process_vm_readv, return the bytes,
     *  otherwise resort to /proc/XXX/mem. */
    if (read >= 0) return read;
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/mem", (int)pid);

    /* open the memory path. */
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;

    /* read into the buffer and return the bytes, if failed check errno. */
    ssize_t n = pread(fd, destination, length, (off_t)address);
    int saved = errno;
    close(fd);
    errno = saved;
    return n;
}

/**
 * @brief initialize a region of memory to be read from a processes' memory; [start, end).
 *
 * @param pid the target process id.
 * @param start the start of the region.
 * @param end the end of the region.
 * @return a pointer to the allocated region if successful, 0x0 o.w.
 */
region_t*
region_init(pid_t pid, uint64_t start, uint64_t end) {
    if (start >= end) return 0x0;

    /* allocate our region buffer. */
    region_t* region = calloc(1u, sizeof *region);
    region->pid = pid;
    region->page.size = 0x1000; /* we read 4KiB at a time. */
    region->base = align_down(start, region->page.size);
    uint64_t end_up = align_up(end, region->page.size);
    uint64_t span = end_up - region->base;
    if (span == 0) return 0x0;

    /* calculate the number of pages and size. */
    region->size = span;
    region->page.count = (region->size + region->page.size - 1) /
        region->page.size;
    region->data = (unsigned char*) calloc(1u, region->size);
    if (!region->data) {
        fprintf(stderr, "tapi, region_init; calloc failed; could not allocate memory for region.");
        free(region);
        return 0x0;
    }
    region->present = (unsigned char*) calloc(region->page.count, 1);
    if (!region->present) {
        fprintf(stderr, "tapi, region_init; calloc failed; could not allocate memory for present.");
        free(region->data);
        free(region);
        return 0x0;
    }
    return region;
};

/**
 * @brief free a region as well as all of its data.
 *
 * @param region the region to be freed.
 */
void
region_free(region_t* region) {
    free(region->data);
    free(region->present);
    free(region);
};

/**
 * @brief read pages into the regions data, marking the present bit per page.
 *
 * @param region the region to be read.
 * @return a count of the number of pages if successful, -1 o.w.
 */
ssize_t
region_read(region_t* region) {
    if (!region) {
        fprintf(stderr, "tapi, region_read; region is null.");
        return -1;
    }

    /* iterate through the number of pages we need to read. */
    ssize_t read = 0;
    for (size_t i = 0; i < region->page.count; i++) {
        uint64_t address = region->base + i * region->page.size;
        unsigned char* destination = region->data + i * region->page.size;
        size_t to_read = region->page.size;
        if ((i + 1) * region->page.size > region->size)
            to_read = region->size - i * region->page.size;
        ssize_t bytes = read_rempr(region->pid, address, destination, to_read);
        if (bytes < 0) {
            region->present[i] = 0;
            continue;
        }

        /* even partial read means "present" for our purposes */
        region->present[i] = 1;
        read++;
    }
    return read;
};