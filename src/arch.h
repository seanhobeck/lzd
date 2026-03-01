/**
 * @author Sean Hobeck
 * @date 2026-02-28
 */
#ifndef LZD_ARCH_H
#define LZD_ARCH_H

/*! @uses cs_mode, cs_arch. */
#include <capstone/capstone.h>

/* ... */
typedef struct {
    /* mode an architecture. */
    cs_arch arch;
    cs_mode mode;
} tup_arch_t;

/** @return get an architecture tuple for the compiled version of lzd. */
static tup_arch_t get_arch() {
#ifdef __x86_64__
    return (tup_arch_t){ .arch = CS_ARCH_X86, .mode = CS_MODE_64 };
#endif
#ifdef __i386__
    return (tup_arch_t){ .arch = CS_ARCH_X86, .mode = CS_MODE_32 };
#endif
#ifdef __aarch64__
    return (tup_arch_t){ .arch = CS_ARCH_AARCH64, .mode = CS_MODE_ARM };
#endif
};
#endif /* LZD_ARCH_H */