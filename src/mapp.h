/**
 * @author Sean Hobeck
 * @date 2026-02-28
 */
#ifndef LZD_MAPP_H
#define LZD_MAPP_H

/*! @uses uint64_t. */
#include <stdint.h>

/*! @uses dyna_t. */
#include "dyna.h"

/*! @uses ssize_t. */
#include <sys/types.h>

/* ... */
typedef struct {
    uint64_t start; /* start, end and offset of the map. */
    uint64_t end;
    uint64_t offset;
    /* permission bits. */
    unsigned r : 1, w : 1, x : 1, p : 1;
    char path[512u]; /* empty if none. */
} map_t;

/**
 * @brief parse the maps of a target process.
 *
 * @param pid the target process pid.
 * @param maps the dynamic array for the maps.
 * @return -1 if a failure occurs, 0 o.w.
 */
ssize_t
parse_maps(pid_t pid, dyna_t* maps);
#endif /* LZD_MAPP_H */