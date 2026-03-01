/**
 * @author Sean Hobeck
 * @date 2026-02-28
 */
#include "disj.h"

/*! @uses ux_insn_t, ux_page_msg_t. */
#include "ux.h"

/*! @uses capstone. */
#include <capstone/capstone.h>

/*! @uses calloc, free, malloc. */
#include <stdlib.h>

/*! @uses fprintf, stderr. */
#include <stdio.h>

/*! @uses memcpy, strncpy. */
#include <string.h>

/*! @uses pthread_key_t, pthread_once_t. */
#include <pthread.h>

/*! @uses internal. */
#include "dyna.h"

/* copied macro for minimum values. */
#define min(a, b) ((a) < (b) ? (a) : (b))

#pragma region cs thread specific helpers
/* ... */
typedef struct {
    tup_arch_t tuple;
    csh handle;
    int ok;
} cs_tls_t;

/* thread specific key for capstone. */
static pthread_key_t g_cs_key;
static pthread_once_t g_cs_once = PTHREAD_ONCE_INIT;

/**
 * @brief free a tls struct.
 *
 * @param p the tls ptr to be freed.
 */
internal void
cs_tls_free(void* p) {
    cs_tls_t* t = p;
    if (!t) return; /* tls already freed. */
    if (t->ok) cs_close(&t->handle);
    free(t);
};

/**
 * @brief initialize a capstone-specific thread key.
 */
internal void
cs_tls_init(void) { pthread_key_create(&g_cs_key, cs_tls_free); };

/**
 * @brief are two architectures equal?
 */
internal bool
tuple_equal(tup_arch_t a, tup_arch_t b) {
    return a.arch == b.arch && a.mode == b.mode;
};

/**
 * @brief open a thread specific tls capstone handle for worker threads.
 *
 * @param tuple the architecture tuple
 * @return a new tls struct containing a thread specific opened handle from capstone.
 */
internal cs_tls_t*
cs_get(tup_arch_t tuple) {
    pthread_once(&g_cs_once, cs_tls_init);

    /* get the thread specific tls. */
    cs_tls_t* tls = pthread_getspecific(g_cs_key);
    if (tls && tls->ok && tuple_equal(tls->tuple, tuple))
        return tls;
    if (!tls) {
        tls = calloc(1u, sizeof *tls);
        if (!tls) {
            fprintf(stderr, "lzd, cs_get; calloc failed; "
                            "could not allocate memory for cs_tls_t.\n");
            return 0x0;
        }
        pthread_setspecific(g_cs_key, tls);
    } else if (tls->ok) {
        cs_close(&tls->handle);
        tls->ok = 0;
    }

    /* copy the tuple and open for disassembly. */
    tls->tuple = tuple;
    if (cs_open(tuple.arch, tuple.mode, &tls->handle) != CS_ERR_OK)
        return NULL;

    tls->ok = 1;
    return tls;
};
#pragma endregion

/**
 * @brief main worker thread for disassembling a byte buffer.
 *
 * @param arg the job to be executed.
 */
internal void
disj_run_bytes(void* arg) {
    disas_job_t *job = arg;

    /* decode with capstone, get the tls handle and get ready to start iterating. */
    cs_tls_t* tls = cs_get(job->tuple);
    if (!tls) { free(job->data); free(job); return; }
    cs_insn *insn = NULL;
    size_t count = cs_disasm(tls->handle, job->data, job->length, job->vaddr, 0, &insn);

    /* iterate through each instruction. */
    dyna_t* disassembled = dyna_create();
    for (size_t i = 0; i < count; i++) {
        /* allocate a decompiled insn. and push the data. */
        ux_insn_t* out = calloc(1u, sizeof *out);
        out->address = insn[i].address;
        out->size = (uint8_t) min(insn[i].size, 16);
        memcpy(out->bytes, insn[i].bytes, out->size);
        strncpy(out->mnemonic, insn[i].mnemonic, sizeof(out->mnemonic) - 1);
        strncpy(out->op_str, insn[i].op_str, sizeof(out->op_str) - 1);
        dyna_push(disassembled, out);
    }
    cs_free(insn, count);

    /* allocate and pack a message, then post. */
    ux_page_msg_t* message = calloc(1, sizeof *message);
    message->pid = 0; /* no pid for byte-based disassembly. */
    message->base = job->vaddr;
    message->length = job->length;
    message->read = job->length;
    message->insns = disassembled;
    ux_post(message); /* handler now owns msg + msg->insns. */

    free(job->data);
    free(job);
};

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
    size_t length, uint64_t vaddr) {
    if (!pool || !data || length == 0) return -1;
    /* allocate and copy the data. */
    uint8_t* data_copy = malloc(length);
    if (!data_copy) {
        fprintf(stderr, "lzd, disj_post_bytes; malloc failed; could not allocate memory for data.\n");
        return -1;
    }
    memcpy(data_copy, data, length);

    /* allocate the job. */
    disas_job_t* job = calloc(1u, sizeof *job);
    if (!job) {
        fprintf(stderr, "lzd, disj_post_bytes; calloc failed; could not allocate memory for job.\n");
        free(data_copy);
        return -1;
    }
    job->tuple = tuple;
    job->data = data_copy;
    job->length = length;
    job->vaddr = vaddr;

    /* post the job. */
    if (wrk_pool_post(pool, disj_run_bytes, job) != 0) {
        free(data_copy);
        free(job);
        fprintf(stderr, "lzd, disj_post_bytes; wrk_pool_post failed; could not post job.\n");
        return -1;
    }
    return 0;
}
