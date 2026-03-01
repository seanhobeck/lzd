/**
 * @author Sean Hobeck
 * @date 2026-02-28
 */
#ifndef LZD_UX_H
#define LZD_UX_H

/*! @uses uint64_t, uint8_t, size_t, pid_t. */
#include <sys/types.h>
#include <stdint.h>

/*! @uses dyna_t. */
#include "dyna.h"

/* ... */
typedef struct {
    uint64_t address;
    uint8_t size;
    uint8_t bytes[16];

    /* used in the tui. */
    char mnemonic[32];
    char op_str[128];
    char* full_string;
} ux_insn_t;

/* ... */
typedef struct {
    uint64_t base; /* chunk base address */
    size_t length; /* visible bytes */
    size_t read; /* bytes read (length + overlap) */
    pid_t pid;
    dyna_t* insns; /* dyna of ux_insn_t (owned by ux thread after post). */
} ux_page_msg_t;

/** @brief initialize the ux module, more specifically the worker pool. */
void ux_init();

/** @brief shutdown the ux module. */
void ux_shutdown();

/**
 * @brief post finished disassembly jobs to the ux module for rendering.
 *
 * @param message a ux page message of decoded insns.
 */
void
ux_post(ux_page_msg_t* message);

/**
 * @brief handle keyboard input for the ux.
 *
 * @param model the ui model.
 * @param character the character input.
 * @return the action to take.
 */
#include "ui.h"
ui_act_t
ux_handle_key(ui_model_t* model, int character);
#endif /* LZD_UX_H */