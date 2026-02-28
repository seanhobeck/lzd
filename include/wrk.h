/**
 * @author Sean Hobeck
 * @date 2026-02-27
 */
#ifndef LZD_WRK_H
#define LZD_WRK_H

/*! @uses pthread_... */
#include <pthread.h>

/*! @uses ... */
#include <stdlib.h>

/*! @uses ... */
#include <stddef.h>

/*! @uses errno. */
#include <errno.h>

/*! @uses dyna_t, dyna_push. */
#include "dyna.h"

/**
 * ...
 */
typedef void (*wrk_fn_t)(void*);

/**
 * ...
 */
typedef struct {
	wrk_fn_t fn;
	void* arg;
} job_t;

/**
 * ...
 */
typedef struct {
	pthread_t *threads;
	size_t nthreads;

	pthread_mutex_t mu;
	pthread_cond_t has_work;
	pthread_cond_t idle;

	dyna_t* jobs;

	size_t queued;
	size_t active;

	int shutting_down;
} wrk_pool_t;

/* create a pool with n worker threads.
 * returns NULL on failure.
 */
wrk_pool_t *wrk_pool_create(size_t nthreads);

/* submit a job to the pool.
 * returns 0 on success, -1 on failure (e.g. pool shutting down / oom).
 */
int wrk_pool_post(wrk_pool_t* pool, wrk_fn_t fn, void* arg);

/* block until queue is empty and no workers are executing a job. */
void wrk_pool_drain(wrk_pool_t* pool);

/* stop accepting new work, wake workers, join all threads.
 * safe to call once.
 */
void wrk_pool_shutdown(wrk_pool_t* pool);

/* shutdown + free. */
void wrk_pool_destroy(wrk_pool_t* pool);
#endif /* LZD_WRK_H */