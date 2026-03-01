/**
 * @author Sean Hobeck
 * @date 2026-02-28
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "wrk.h"

#include "emit.h"

int main(void) {
	// Load binary once
	emit_ctx_t* ctx = emit_load("./lit", get_arch());
	emit_scan_text(ctx); // Find code ranges

	wrk_pool_t* pool = wrk_pool_create(4);

	// As user scrolls, emit jobs for visible range
	emit_range(ctx, pool, 0x401000, 0x402000);

	// Or disassemble everything
	emit_all(ctx, pool);

	wrk_pool_drain(pool);
	emit_free(ctx);
	return 0;
}