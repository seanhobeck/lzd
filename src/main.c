/**
 * @author Sean Hobeck
 * @date 2026-02-29
 */
#include <stdio.h>

/*! @uses ui_model_t, ui_model_create, ui_run, ui_model_free. */
#include "ui.h"

/*! @uses ux_init. */
#include "ux.h"

/* reference to the ui model. */
ui_model_t* g_ui_model = NULL;

int main() {
    ux_init();
    g_ui_model = ui_model_create("lzd - lazy disassembler", "? | ?");
    ui_run(g_ui_model);
    ui_model_free(g_ui_model);
}