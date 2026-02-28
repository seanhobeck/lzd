/**
 * @author Sean Hobeck
 * @date 2026-02-28
 */
#include "wrk.h"

/*! @uses fprintf, stderr. */
#include <stdio.h>

/*! @uses calloc, free. */
#include <stdlib.h>

/*! @uses size_t. */
#include <stddef.h>

/*! @uses internal. */
#include "dyna.h"

/**
 * @brief make a new job_t structure for a worker thread to execute.
 *
 * @param fn the function for the worker to execute.
 * @param arg the paramter for the job.
 * @return a new pointer to a job made for the function.
 */
internal job_t*
job_make(wrk_fn_t fn, void* arg) {
	/* allocate and copy. */
    job_t* job = calloc(1, sizeof *job);
    if (!job) {
    	fprintf(stderr, "lzd, job_make; calloc failed; could not allocate memory for job.");
        return 0x0;
    }
    job->fn = fn;
    job->arg = arg;
    return job;
}

/**
 * @brief the main code executor for a worker thread; grab a job and execute outside of lock.
 *
 * @param arg a pointer to the worker pool.
 * @return 0x0.
 */
internal void*
wrk_main(void* arg) {
    wrk_pool_t* pool = arg;
    for (;;) {
        pthread_mutex_lock(&pool->lock);
        while (!pool->shutting_down && pool->job_queue->count == 0u) {
			pthread_cond_wait(&pool->has_work, &pool->lock);
        }
        if (pool->shutting_down && pool->job_queue->count == 0u) {
			pthread_mutex_unlock(&pool->lock);
			break;
        }

    	/* grab a job and starting executing it. */
        job_t* job = ring_pop(pool->job_queue);
        pool->active++;
        pthread_mutex_unlock(&pool->lock);

        /* run outside lock. */
        if (job && job->fn)
			job->fn(job->arg);
        free(job);
        pthread_mutex_lock(&pool->lock);
        pool->active--;

    	/* if there are no jobs queued or active, we broadcast that we are idle. */
        if (pool->queued == 0u && pool->active == 0u)
			pthread_cond_broadcast(&pool->idle);
        pthread_mutex_unlock(&pool->lock);
    }
    return 0x0;
}

/**
 * @brief create a new worker pool with a set number of threads.
 *
 * @param count the number of worker threads to be created.
 * @return a pointer to a worker pool ready for jobs to be posted.
 */
wrk_pool_t*
wrk_pool_create(size_t count) {
    if (count == 0) count = 1;

	/* initialize the pool as well as the threads, mutex lock, and conditions. */
    wrk_pool_t* pool = calloc(1, sizeof *pool);
    if (!pool) {
    	fprintf(stderr, "lzd, wrk_pool_create; calloc failed; could not allocate memory for pool.");
	    return 0x0;
    }
    pool->threads = (pthread_t*) calloc(count, sizeof(pthread_t));
    if (!pool->threads) {
    	fprintf(stderr, "lzd, wrk_pool_create; calloc failed; could not allocate memory for threads.");
        free(pool);
        return 0x0;
    }
    pool->count = count;
    pthread_mutex_init(&pool->lock, 0x0);
    pthread_cond_init(&pool->has_work, 0x0);
    pthread_cond_init(&pool->idle, 0x0);

	/* queue cap of 8 jobs, we don't need more than this. */
	if ((pool->job_queue = ring_init()) == 0x0) {
		pthread_cond_destroy(&pool->idle);
		pthread_cond_destroy(&pool->has_work);
		pthread_mutex_destroy(&pool->lock);
		free(pool->threads);
		free(pool);
		return NULL;
	}

	/* iterate and create a thread, if one cannot be created, destroy the entire pool and report
	 *  the error. */
    for (size_t i = 0; i < count; i++) {
        int retval = pthread_create(&pool->threads[i], 0x0, wrk_main, pool);
        if (retval != 0) {
            /* if partial create, shut down the ones created. */
        	fprintf(stderr, "lzd, wrk_pool_create; fatal pthread_create failed; could not create "
							"worker thread(s).");
            pthread_mutex_lock(&pool->lock);
            pool->shutting_down = 1;
            pthread_cond_broadcast(&pool->has_work);
            pthread_mutex_unlock(&pool->lock);
            for (size_t j = 0; j < i; j++)
            	pthread_join(pool->threads[j], 0x0);
            pthread_cond_destroy(&pool->idle);
            pthread_cond_destroy(&pool->has_work);
            pthread_mutex_destroy(&pool->lock);
            free(pool->threads);
            free(pool);
            return 0x0;
        }
    }
    return pool;
}

/**
 * @brief 'post' or submit a new job to the worker pool.
 *
 * @param pool the pool to post a new job to.
 * @param fn the function to be executed.
 * @param arg the argument required for the job.
 * @return -1 if a failure occurs, 0 o.w.
 */
ssize_t
wrk_pool_post(wrk_pool_t* pool, const wrk_fn_t fn, void* arg) {
	if (!pool || !fn) return -1;

	/* make a new job given the function and argument. */
	job_t* job = job_make(fn, arg);
	if (!job) return -1;
	pthread_mutex_lock(&pool->lock);
	if (pool->shutting_down) {
		pthread_mutex_unlock(&pool->lock);
		free(job);
		return -1;
	}

	/* push the job onto the ring queue and we are done. */
	ring_push(pool->job_queue, job);
	pthread_cond_signal(&pool->has_work);
	pthread_mutex_unlock(&pool->lock);
	return 0u;
}

/**
 * @brief drain a worker pool of all jobs.
 *
 * @param pool the pool to be drained.
 */
void
wrk_pool_drain(wrk_pool_t* pool) {
	if (!pool) return;

	/* wait until each worker thread is done, then continue. */
	pthread_mutex_lock(&pool->lock);
	while (pool->queued != 0 || pool->active != 0)
		pthread_cond_wait(&pool->idle, &pool->lock);
	pthread_mutex_unlock(&pool->lock);
}

/**
 * @brief shutdown all threads in the pool by joining them.
 *
 * @param pool the pool to be shutdown.
 */
void
wrk_pool_shutdown(wrk_pool_t* pool) {
	if (!pool) return;

	/* change the condition. */
	pthread_mutex_lock(&pool->lock);
	if (!pool->shutting_down) {
		pool->shutting_down = true;
		pthread_cond_broadcast(&pool->has_work);
	}
	pthread_mutex_unlock(&pool->lock);

	/* join each worker thread. */
	for (size_t i = 0; i < pool->count; i++)
		pthread_join(pool->threads[i], 0x0);
}

/**
 * @brief destroy a worker pool for good.
 *
 * @param pool the worker pool to be destroyed.
 */
void
wrk_pool_destroy(wrk_pool_t* pool) {
	/* shutdown the pool. */
	if (!pool) return;
	wrk_pool_shutdown(pool);

	/* free any queued jobs (should be none, but be safe) */
	for (;;) {
		job_t* job = 0x0;
		pthread_mutex_lock(&pool->lock);
		job = ring_pop(pool->job_queue);
		pthread_mutex_unlock(&pool->lock);
		if (!job) break;
		free(job);
	}

	/* destroy thread conditions and mutex lock. */
	pthread_cond_destroy(&pool->idle);
	pthread_cond_destroy(&pool->has_work);
	pthread_mutex_destroy(&pool->lock);
	free(pool->threads);
	free(pool);
}