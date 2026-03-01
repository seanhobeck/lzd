/**
 * @author Sean Hobeck
 * @date 2026-02-28
 */
#ifndef LZD_EMIT_H
#define LZD_EMIT_H

/*! @uses uint64_t, uint8_t, size_t. */
#include <stdint.h>

/*! @uses bool. */
#include <stdbool.h>

/*! @uses tup_arch_t. */
#include "arch.h"

/*! @uses wrk_pool_t. */
#include "wrk.h"

/*! @uses dyna_t. */
#include "dyna.h"

/*! @uses elf_t, elf_shdr_t. */
#include "elfx.h"

/* represents a loaded elf binary with its .text section in memory. */
typedef struct {
    elf_t* elf; /* parsed elf structure. */
    tup_arch_t tuple; /* architecture for disassembly. */
    uint8_t* text_data; /* .text section bytes. */
    uint64_t text_vaddr; /* virtual address of .text. */
    size_t text_size; /* size of .text section. */
    dyna_t* code_ranges; /* dynamic array of code_range_t*. */
} emit_ctx_t;

/* represents a contiguous range of executable code. */
typedef struct {
    uint64_t vaddr; /* virtual address. */
    size_t offset; /* offset into text_data. */
    size_t length; /* length of code range. */
} code_range_t;

/**
 * @brief load an elf binary and prepare it for disassembly.
 *
 * @param path the path to the elf binary.
 * @param tuple the architecture tuple (pass {0, 0} to auto-detect from ELF).
 * @return an emit context if successful, 0x0 o.w.
 */
emit_ctx_t*
emit_load(const char* path, tup_arch_t tuple);

/**
 * @brief free an emit context.
 *
 * @param ctx the context to be freed.
 */
void
emit_free(emit_ctx_t* ctx);

/**
 * @brief scan .text section and identify code ranges (skip nops/padding).
 *
 * @param ctx the emit context.
 * @return -1 if a failure occurs, 0 o.w.
 */
ssize_t
emit_scan_text(emit_ctx_t* ctx);

/**
 * @brief emit disassembly jobs for a specific virtual address range.
 *
 * @param ctx the emit context.
 * @param pool the worker pool to post jobs to.
 * @param vaddr_start the starting virtual address.
 * @param vaddr_end the ending virtual address.
 * @return -1 if a failure occurs, 0 o.w.
 */
ssize_t
emit_range(emit_ctx_t* ctx, wrk_pool_t* pool, uint64_t vaddr_start, uint64_t vaddr_end);

/**
 * @brief emit all disassembly jobs for the entire binary.
 *
 * @param ctx the emit context.
 * @param pool the worker pool to post jobs to.
 * @return -1 if a failure occurs, 0 o.w.
 */
ssize_t
emit_all(emit_ctx_t* ctx, wrk_pool_t* pool);
#endif /* LZD_EMIT_H */
