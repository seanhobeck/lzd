/**
 * @author Sean Hobeck
 * @date 2026-02-28
 */
#include "mapp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief parse the maps of a target process.
 *
 * @param pid the target process pid.
 * @param maps the dynamic array for the maps.
 * @return -1 if a failure occurs, 0 o.w.
 */
ssize_t
parse_maps(pid_t pid, dyna_t* maps) {
    /* find and open the /proc/XXX/maps file for a given process id. */
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/maps", (int)pid);
    FILE* file = fopen(path, "r");
    if (!file) {
        fprintf(stderr, "lzd, parse_maps; could not open file %s.\n", path);
        return -1;
    }

    /* iterate through each map... */
    char line[1024] = { 0u };
    while (fgets(line, sizeof(line), file)) {
        uint64_t start = 0u, end = 0u, offset = 0u;
        char map_perms[8] = { 0u };
        char map_path[512] = { 0u };

        /* should be using strtoul. */
        int read = sscanf(line, "%lx-%lx %7s %lx %*s %*s %511[^\n]", \
            &start, &end, map_perms, &offset, map_path);
        if (read < 4) continue;

        /* allocate a new map. */
        map_t* map = calloc(1u, sizeof *map);
        map->start = start;
        map->end = end;
        map->offset = offset;
        map->r = map_perms[0] == 'r';
        map->w = map_perms[1] == 'w';
        map->x = map_perms[2] == 'x';
        map->p = map_perms[3] == 'p';

        /* trim leading spaces from path and null-terminate. then push. */
        if (read >= 5u) {
            const char *p = map_path;
            while (*p == ' ' || *p == '\t') p++;
            strncpy(map->path, p, sizeof(map->path) - 1u);
            map->path[sizeof(map->path) - 1] = 0u;
        } else {
            map->path[0] = 0u;
        }
        dyna_push(maps, map);
    }
    fclose(file);
    return 0u;
};