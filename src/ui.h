/**
 * @author Sean Hobeck
 * @date 2026-02-28
 */
#ifndef LZD_UI_H
#define LZD_UI_H

/*! @uses ssize_t. */
#include <sys/types.h>

/*! @uses dyna_t. */
#include "dyna.h"

/*! @uses pthread_mutex_t. */
#include <pthread.h>

/* ... */
typedef enum {
    TUI_ACT_NONE = 0x0,
    TUI_ACT_QUIT, /* quit the application. */
    TUI_ACT_ENTER, /* enter a command. */
    TUI_ACT_REFRESH, /* refresh the tab. */
    TUI_ACT_OPEN, /* open a executable for disassembly. */
} ui_act_t;

/* ... */
typedef enum {
    UI_VIEW_INSTRUCTIONS = 0u, /* show disassembly. */
    UI_VIEW_STRINGS, /* show strings. */
    UI_VIEW_SYMBOLS, /* show symbols. */
} ui_view_mode_t;

/* ... */
typedef struct {
    char* title; /* e.g. "lzd - lazy disassembler". */
    char* subtitle; /* e.g. "x86_64 | ELF64 | ./example_binary". */
    dyna_t* instructions; /* dynamic array of ux_insn_t* from disassembly. */
    dyna_t* strings; /* dynamic array of char* strings extracted from binary. */
    dyna_t* symbols; /* dynamic array of elf symbols from the executable. */
    ui_view_mode_t view_mode; /* current view mode. */
    ssize_t selected; /* which line is "selected". */
    ssize_t scroll; /* first visible line. */
    char cmd[256]; /* command bar text (editable). */
    char status[256]; /* status text (read-only). */
    pthread_mutex_t lock; /* protect concurrent access. */
} ui_model_t;

/**
 * @brief create a new ui model.
 *
 * @param title the title string (will be copied).
 * @param subtitle the subtitle string (will be copied).
 * @return a pointer to an allocated ui model if successful, 0x0 o.w.
 */
ui_model_t*
ui_model_create(const char* title, const char* subtitle);

/**
 * @brief free a ui model.
 *
 * @param model the model to be freed.
 */
void
ui_model_free(ui_model_t* model);

/**
 * @brief add instructions to the ui model.
 *
 * @param model the ui model.
 * @param insns dynamic array of ux_insn_t*.
 */
void
ui_model_add_insns(ui_model_t* model, dyna_t* insns);

/**
 * @brief clear all instructions from the ui model.
 *
 * @param model the ui model.
 */
void
ui_model_clear(ui_model_t* model);

/**
 * @brief add strings to the ui model.
 *
 * @param model the ui model.
 * @param strings dynamic array of char* strings.
 */
void
ui_model_add_strings(ui_model_t* model, dyna_t* strings);

/**
 * @brief add elf symbol strings to the ui model.
 *
 * @param model the ui model.
 * @param symbols dynamic array of elf_symbol_t*.
 */
void
ui_model_add_symbols(ui_model_t* model, dyna_t* symbols);

/**
 * @brief set the view mode.
 *
 * @param model the ui model.
 * @param mode the new view mode.
 */
void
ui_model_set_view(ui_model_t* model, ui_view_mode_t mode);

/**
 * @brief run the ui event loop (blocking).
 *
 * @param model the ui model.
 * @return the action that caused the loop to exit.
 */
ui_act_t
ui_run(ui_model_t* model);
#endif /* LZD_UI_H */
