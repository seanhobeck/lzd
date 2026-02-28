/**
 * @author Sean Hobeck
 * @date 2026-02-28
 */
#include <stdio.h>

#include "wrk.h"

#include <stdlib.h>

#include <unistd.h>

#include "targ.h"

void job(void *arg) {
	int v = *(int *)arg;
	printf("job %d on thread\n", v);
	sleep(1);
	free(arg);
}

int main(void) {
	int pid = target_search_by_name("flatpak-portal");
	printf("target found: %d\n", pid);

	wrk_pool_t *p = wrk_pool_create(4);

	for (int i = 0; i < 1000; i++) {
		int *x = malloc(sizeof(*x));
		*x = i;
		wrk_pool_post(p, job, x);
	}
	wrk_pool_drain(p);
	wrk_pool_destroy(p);
	return 0;
}