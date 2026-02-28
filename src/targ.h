/**
 * @author Sean Hobeck
 * @date 2026-02-28
 */
#ifndef LZD_TARG_H
#define LZD_TARG_H

/*! @uses pid_t. */
#include <stdlib.h>

/**
 * @brief find a target processes pid based on its exact name.
 *
 * @param process_name the target process name to look for.
 * @return a pid if found, -1 o.w.
 */
pid_t
target_search_by_name(const char* process_name);
#endif /* LZD_TARG_H */