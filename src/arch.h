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
#endif /* LZD_ARCH_H */