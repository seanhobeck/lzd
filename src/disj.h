/**
 * @author Sean Hobeck
 * @date 2026-02-28
 */
#ifndef LZD_DISJ_H
#define LZD_DISJ_H

/*! @uses uint64_t, size_t. */
#include <stdint.h>

/*! @uses pid_t. */
#include <sys/types.h>

/*! @uses tup_arch_t. */
#include "arch.h"

typedef struct {
    pid_t pid; /* target proc. id. */
    tup_arch_t tuple; /* architecture tuple for capstone. */
    uint64_t addr; /* starting address. */
    size_t length, read; /* base length, + overlap. */
} disas_job_t;

#endif /* LZD_DISJ_H */