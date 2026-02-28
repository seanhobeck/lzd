/**
 * @author Sean Hobeck
 * @date 2026-02-28
 */
#include "targ.h"

/*! @uses opendir, readdir, closedir, DIR. */
#include <dirent.h>

/*! @uses isdigit. */
#include <ctype.h>

/*! @uses fprintf, stderr. */
#include <stdio.h>

/*! @uses strlen, strchr, strcmp, memcpy. */
#include <string.h>

/*! @uses atoi, pid_t. */
#include <stdlib.h>

/*! @uses bool, true, false. */
#include <stdbool.h>

/*! @uses internal. */
#include "dyna.h"

/** @return if the string provided is just digits. */
internal bool
is_digits(const char* string) {
    for (; *string; string++)
        if (!isdigit(*string)) return false;
    return true;
}

/**
 * @brief read a line from a file into the buffer of a set size (256).
 *
 * @param path the path of the file to read a line from.
 * @param buffer the buffer to read into.
 * @return -1 if a failure occurs, 0 o.w.
 */
internal ssize_t
readln(const char* path, char* buffer) {
    /* open the file. */
    FILE* file = fopen(path, "r");
    if (!file) {
        fprintf(stderr, "lzd, readln; could not open file %s.\n", path);
        return -1;
    }

    /* attempt to read all of the data. */
    if (!fgets(buffer, 256u, file)) {
        fprintf(stderr, "lzd, readln; could not read from file %s.\n", path);
        fclose(file);
        return -1;
    }
    fclose(file);

    /* trim newline(s) etc... */
    size_t size = strlen(buffer);
    while (size && (buffer[size - 1] == '\n' || buffer[size - 1] == '\r')) buffer[--size] = 0x0;
    return 0u;
}

/**
 * @brief read a line of size 2048 from /proc/XXX/cmdline.
 *
 * @param pid the process id to read from.
 * @param buffer the buffer to read /proc/XXX/cmdline into.
 * @return -1 if a failure occurs, 0 o.w.
 */
internal ssize_t
readcln(pid_t pid, char* buffer) {
    /* start reading the process cmdline. */
    char path[64];
    snprintf(path, sizeof path, "/proc/%d/cmdline", pid);
    FILE* file = fopen(path, "rb");
    if (!file) {
        fprintf(stderr, "lzd, readcln; could not open file %s\n.", path);
        return -1;
    }

    /* read the bytes from the cmdline. */
    size_t read = fread(buffer, 1u, 2048u - 1u, file);
    fclose(file);
    if (read == 0u) {
        buffer[0] = 0u;
        return 0u; /* kernel threads have 0x0 cmdline. */
    }
    buffer[read] = 0u; /* null term. */

    /* convert null separators to spaces. */
    for (size_t i = 0; i < read; i++) {
        if (buffer[i] == 0) buffer[i] = ' ';
    }

    /* trim space(s) etc... */
    while (read && buffer[read - 1u] == ' ') buffer[--read] = 0u;
    return 0u;
}

/** @return the name of a process after '/'. */
internal const char*
getbname(const char *s) {
    const char *p = strrchr(s, '/');
    return p ? (p + 1u) : s;
}

/**
 * @brief check if a process name matches a pid based on /proc/XXX/comm|cmdline.
 *
 * @param process_name the process name to be compared.
 * @param comm ...
 * @param cmdline ...
 * @return if the names match.
 */
internal bool
match_prnm(const char *process_name, const char *comm, const char *cmdline) {
    if (strcmp(comm, process_name) == 0u) return true;

    /* cmdline starts with argv0 then a space (after pretty-print). */
    char argv0[512] = { 0u };
    const char *sp = strchr(cmdline, ' ');
    size_t n = sp ? (size_t)(sp - cmdline) : strlen(cmdline);
    if (n >= sizeof(argv0)) n = sizeof(argv0) - 1u;
    memcpy(argv0, cmdline, n);
    argv0[n] = 0u;

    /* if the base name matches... */
    if (strcmp(getbname(argv0), process_name) == 0u) return true;
    return false;
}

/**
 * @brief search for a target process name in the process table.
 *
 * @param target the target process name to search for.
 * @param out_pid a pointer out to the pid.
 * @return -1 if a failure occurs, 0 if not found, and 1 if found.
 */
internal ssize_t
find_pid_by_name(const char *target, pid_t* out_pid) {
    if (!target || !*target) {
        fprintf(stderr, "lzd, lzd_find_pids_by_name; needle is empty.\n");
        return -1;
    }

    /* iterate through the /proc dir. */
    DIR* proc_dir = opendir("/proc");
    if (!proc_dir) {
        fprintf(stderr, "lzd, lzd_find_pids_by_name; could not open /proc. this should NOT "
                        "happen.\n");
        return -1;
    }
    ssize_t found = 0u;
    struct dirent *entry = NULL;
    while ((entry = readdir(proc_dir)) != NULL) {
        /* conver the name of the directory into its pid. */
        if (!is_digits(entry->d_name)) continue;
        pid_t pid = (int) strtol(entry->d_name, 0x0, 10);

        /* get the comm and cmdline paths. */
        char comm_path[64];
        snprintf(comm_path, sizeof(comm_path), "/proc/%d/comm", (int)pid);
        char comm[256] = { 0u };
        char cmdline[2048] = { 0u };

        /* comm is usually readable. if not, skip. */
        if (readln(comm_path, comm) != 0) continue;

        /* cmdline may be empty or permission denied, treat as empty */
        readcln(pid, cmdline);
        if (match_prnm(target, comm, cmdline)) {
            /* if we found a match. */
            *out_pid = pid;
            found = 1u;
            break;
        }
    }
    closedir(proc_dir);
    return found;
}

/**
 * @brief find a target processes pid based on its exact name.
 *
 * @param process_name the target process name to look for.
 * @return a pid if found, -1 o.w.
 */
pid_t
target_search_by_name(const char* process_name) {
    pid_t pid = -1;
    ssize_t retval = find_pid_by_name(process_name, &pid);

    /* if there was an error or not found. */
    if (retval < 0)
        return -2;
    if (retval == 0) {
        fprintf(stderr, "lzd, target_search_by_name; could not find process %s.\n", process_name);
        return -1;
    }
    return pid;
}