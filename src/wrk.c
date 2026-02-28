/**
 * @author Sean Hobeck
 * @date 2026-02-27
 */
#include <wrk.h>

internal void
job_free(job_t *j) {
  free(j);
}

internal job_t*
job_make(wrk_fn_t fn, void *arg) {
    job_t *job = calloc(1, sizeof *job);
    if (!job) {
        return NULL;
    }
    job->fn = fn;
    job->arg = arg;
    return job;
}

internal void*
worker_main(void *arg) {
    wrk_pool_t *p = arg;
    for (;;) {
        pthread_mutex_lock(&p->mu);

        while (!p->shutting_down && p->head == NULL) {
          pthread_cond_wait(&p->has_work, &p->mu);
        }

        if (p->shutting_down && p->head == NULL) {
          pthread_mutex_unlock(&p->mu);
          break;
        }

        job_t *j = 0x0;
        p->active++;
        pthread_mutex_unlock(&p->mu);

        /* run outside lock */
        if (j && j->fn) {
          j->fn(j->arg);
        }
        job_free(j);

        pthread_mutex_lock(&p->mu);
        p->active--;
        if (p->queued == 0 && p->active == 0) {
          pthread_cond_broadcast(&p->idle);
        }
        pthread_mutex_unlock(&p->mu);
    }

    return NULL;
}

wrk_pool_t *worker_pool_create(size_t nthreads) {
    if (nthreads == 0) nthreads = 1;

    wrk_pool_t *p = (wrk_pool_t *)calloc(1, sizeof(*p));
    if (!p) return NULL;

    p->threads = (pthread_t *)calloc(nthreads, sizeof(pthread_t));
    if (!p->threads) {
        free(p);
        return NULL;
    }

    p->nthreads = nthreads;

    pthread_mutex_init(&p->mu, NULL);
    pthread_cond_init(&p->has_work, NULL);
    pthread_cond_init(&p->idle, NULL);

    for (size_t i = 0; i < nthreads; i++) {
        int rc = pthread_create(&p->threads[i], NULL, worker_main, p);
        if (rc != 0) {
            /* if partial create, shut down the ones created */
            pthread_mutex_lock(&p->mu);
            p->shutting_down = 1;
            pthread_cond_broadcast(&p->has_work);
            pthread_mutex_unlock(&p->mu);

            for (size_t j = 0; j < i; j++) {
              pthread_join(p->threads[j], NULL);
            }

            pthread_cond_destroy(&p->idle);
            pthread_cond_destroy(&p->has_work);
            pthread_mutex_destroy(&p->mu);

            free(p->threads);
            free(p);
            return NULL;
        }
    }

    return p;
}

int worker_pool_submit(wrk_pool_t *p, wrk_fn_t fn, void *arg) {
	if (!p || !fn) return -1;

	job_t *j = job_make(fn, arg);
	if (!j) return -1;

	pthread_mutex_lock(&p->mu);
	if (p->shutting_down) {
		pthread_mutex_unlock(&p->mu);
		job_free(j);
		return -1;
	}

	queue_push(p, j);
	pthread_cond_signal(&p->has_work);
	pthread_mutex_unlock(&p->mu);
	return 0;
}

void worker_pool_drain(wrk_pool_t *p) {
	if (!p) return;

	pthread_mutex_lock(&p->mu);
	while (p->queued != 0 || p->active != 0) {
		pthread_cond_wait(&p->idle, &p->mu);
	}
	pthread_mutex_unlock(&p->mu);
}

void worker_pool_shutdown(wrk_pool_t *p) {
	if (!p) return;

	pthread_mutex_lock(&p->mu);
	if (!p->shutting_down) {
		p->shutting_down = 1;
		pthread_cond_broadcast(&p->has_work);
	}
	pthread_mutex_unlock(&p->mu);

	for (size_t i = 0; i < p->nthreads; i++) {
		pthread_join(p->threads[i], NULL);
	}
}

void worker_pool_destroy(wrk_pool_t *p) {
	if (!p) return;

	worker_pool_shutdown(p);

	/* free any queued jobs (should be none, but be safe) */
	job_t *cur = p->head;
	while (cur) {
		job_t *n = cur->next;
		job_free(cur);
		cur = n;
	}

	pthread_cond_destroy(&p->idle);
	pthread_cond_destroy(&p->has_work);
	pthread_mutex_destroy(&p->mu);

	free(p->threads);
	free(p);
}