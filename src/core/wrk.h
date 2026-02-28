/**
 * @author Sean Hobeck
 * @date 2026-02-27
 */
#ifndef LZD_WRK_H
#define LZD_WRK_H

/*! @uses pthread_... */
#include <pthread.h>

/*! @uses bool, true, false. */
#include <stdbool.h>

/*! @uses ring_t, ring_push, ring_pop. */
#include "../ring.h"

/* ... */
typedef void (*wrk_fn_t)(void*);

/*
 * ...
 */
typedef struct {
	wrk_fn_t fn;
	void* arg;
} job_t;

/*
 * ...
 */
typedef struct {
	pthread_t *threads;
	size_t count;
	pthread_mutex_t lock;
	pthread_cond_t has_work;
	pthread_cond_t idle;
	ring_t* job_queue;
	size_t queued;
	size_t active;
	bool shutting_down;
} wrk_pool_t;

/**
 * @brief create a new worker pool with a set number of threads.
 *
 * @param count the number of worker threads to be created.
 * @return a pointer to a worker pool ready for jobs to be posted.
 */
wrk_pool_t*
wrk_pool_create(size_t count);

/**
 * @brief 'post' or submit a new job to the worker pool.
 *
 * @param pool the pool to post a new job to.
 * @param fn the function to be executed.
 * @param arg the argument required for the job.
 * @return -1 if a failure occurs, 0 o.w.
 */
ssize_t
wrk_pool_post(wrk_pool_t* pool, wrk_fn_t fn, void* arg);

/**
 * @brief drain a worker pool of all jobs.
 *
 * @param pool the pool to be drained.
 */
void
wrk_pool_drain(wrk_pool_t* pool);

/**
 * @brief shutdown all threads in the pool by joining them.
 *
 * @param pool the pool to be shutdown.
 */
void
wrk_pool_shutdown(wrk_pool_t* pool);

/**
 * @brief destroy a worker pool for good.
 *
 * @param pool the worker pool to be destroyed.
 */
void
wrk_pool_destroy(wrk_pool_t* pool);
#endif /* LZD_WRK_H */