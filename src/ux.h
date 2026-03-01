/**
 * @author Sean Hobeck
 * @date 2026-02-28
 */
#ifndef LZD_UX_H
#define LZD_UX_H

/*! @uses uint64_t, uint8_t, size_t, pid_t. */
#include <sys/types.h>
#include <stdint.h>

/*! @uses dyna_t. */
#include "dyna.h"

/* ... */
typedef struct {
    uint64_t address;
    uint8_t size;
    uint8_t bytes[16];

    /* used in the tui. */
    char mnemonic[32];
    char op_str[128];
} ux_insn_t;

/* ... */
typedef struct {
    /* copy of job metadata (whatever you want UI to know) */
    uint64_t base; /* chunk base address */
    size_t length; /* visible bytes */
    size_t read; /* bytes read (length + overlap) */
    pid_t pid;
    dyna_t* insns; /* dyna of ux_insn_t (owned by ux thread after post). */
} ux_page_msg_t;

/**
 * @brief post finished disassembly jobs to the ux module for rendering.
 *
 * @param message a ux page message of decoded insns.
 */
void
ux_post(ux_page_msg_t* message);
#endif /* LZD_UX_H */