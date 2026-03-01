/**
 * @author Sean Hobeck
 * @date 2026-02-28
 */
#ifndef LZD_DISJ_H
#define LZD_DISJ_H

/*! @uses uint64_t, uint8_t, size_t. */
#include <stdint.h>

/*! @uses pid_t. */
#include <sys/types.h>

/*! @uses tup_arch_t. */
#include "arch.h"

/*! @uses wrk_pool_t, wrk_pool_post. */
#include "wrk.h"

typedef struct {
    tup_arch_t tuple; /* architecture tuple for capstone. */
    uint8_t* data; /* byte buffer to disassemble. */
    size_t length; /* length of the buffer. */
    uint64_t vaddr; /* virtual address of the first byte. */
} disas_job_t;

/**
 * @brief post a byte buffer to be disassembled by worker threads.
 *
 * @param pool the worker pool to post the job to.
 * @param tuple the architecture tuple.
 * @param data the byte buffer (will be copied).
 * @param length the length of the buffer.
 * @param vaddr the virtual address of the first byte.
 * @return -1 if a failure occurs, 0 o.w.
 */
ssize_t
disj_post_bytes(wrk_pool_t* pool, tup_arch_t tuple, uint8_t* data, \
    size_t length, uint64_t vaddr);
#endif /* LZD_DISJ_H */
