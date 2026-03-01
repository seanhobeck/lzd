/**
 * @author Sean Hobeck
 * @date 2026-02-28
 */
#include "ux.h"

/*! @uses fprintf, stderr, snprintf. */
#include <stdio.h>

/*! @uses calloc, free. */
#include <stdlib.h>

/*! @uses strnlen. */
#include <string.h>

/*! @uses ncurses. */
#include <ncurses.h>

/*! @uses ui_action_t. */
#include "ui.h"

/*! @uses internal. */
#include "dyna.h"

/*! @uses wrk_pool_t, wrk_pool_create, wrk_pool_drain. */
#include "wrk.h"

/*! @uses emit_ctx_t, emit_load, emit_scan_text, emit_all. */
#include "emit.h"

/* a reference to the work pool. */
static wrk_pool_t* g_wrk_pool;

/* a reference to a global emit context. */
static emit_ctx_t* g_ctx;

/**
 * @brief format a single instruction into a string for display.
 *
 * @param insn the instruction to format.
 * @return formatted string (caller must free), or 0x0 on failure.
 */
internal char*
format_insn(ux_insn_t* insn) {
    if (!insn) return 0x0;

    /* allocate buffer for formatted line. */
    char* line = calloc(1u, 512u);
    if (!line) {
        fprintf(stderr, "lzd, format_insn; calloc failed; could not allocate memory for line.\n");
        return 0x0;
    }

    /* format: 0x401000: 48 89 e5 48 83 ec 20          mov rbp, rsp */
    size_t offset = 0;

    /* address column (12 chars). */
    offset += snprintf(line + offset, 512u - offset, "0x%08lx:  ", insn->address);

    /* bytes column (max 16 bytes = 48 chars for hex + spaces). */
    for (uint8_t i = 0; i < 16; i++) {
        if (i < insn->size)
            offset += snprintf(line + offset, 512u - offset, "%02x ", insn->bytes[i]);
        else
            offset += snprintf(line + offset, 512u - offset, "   ");
    }

    /* separator between bytes and mnemonic. */
    offset += snprintf(line + offset, 512u - offset, " ");

    /* mnemonic and operands. */
    if (insn->mnemonic[0])
        offset += snprintf(line + offset, 512u - offset, "%s", insn->mnemonic);
    if (insn->op_str[0])
        offset += snprintf(line + offset, 512u - offset, " %s", insn->op_str);

    return line;
}

/** @brief initialize the ux module, more specifically the worker pool. */
void
ux_init() {
    g_wrk_pool = wrk_pool_create(4); /* max of 4 worker threads. */
};

/** @brief shutdown the ux module. */
void
ux_shutdown() {
    wrk_pool_drain(g_wrk_pool);
    emit_free(g_ctx);
}

/**
 * @brief post finished disassembly jobs to the ux module for rendering.
 *
 * @param message a ux page message of decoded insns.
 */
void
ux_post(ux_page_msg_t* message) {
    if (!message) return;

    /* format each instruction into a display string. */
    _foreach(message->insns, ux_insn_t*, insn)
        insn->full_string = format_insn(insn);
        if (!insn->full_string) {
            fprintf(stderr, "lzd, ux_post; format_insn failed for address 0x%lx.\n", insn->address);
            continue;
        }
    _endforeach;

    /* store in global ui model. */
    extern ui_model_t* g_ui_model;
    if (g_ui_model) {
        ui_model_add_insns(g_ui_model, message->insns);
    }

    /* free the message (instructions now owned by model). */
    dyna_free(message->insns);
    free(message);
}

/**
 * @brief handle keyboard input for the ux.
 *
 * @param model the ui model.
 * @param character the character input.
 * @return the action to take.
 */
ui_act_t
ux_handle_key(ui_model_t* model, int character) {
    int cmdlen = (int)strnlen(model->cmd, sizeof(model->cmd) - 1);

    /* iterate through each character. */
    switch (character) {
        case KEY_UP: {
            if (model->selected > 0) model->selected--;
            return TUI_ACT_NONE;
        }
        case KEY_DOWN: {
            if (model->selected < (ssize_t)model->instructions->length - 1) model->selected++;
            return TUI_ACT_NONE;
        }
        case KEY_PPAGE: /* page up. */ {
            model->selected -= 10;
            if (model->selected < 0) model->selected = 0;
            return TUI_ACT_NONE;
        }
        case KEY_NPAGE: /* page down. */ {
            model->selected += 10;
            if (model->selected >= (ssize_t)model->instructions->length)
                model->selected = (ssize_t)model->instructions->length - 1;
            return TUI_ACT_NONE;
        }
        case '\n':
        case KEY_ENTER: /* perform action. */ {
            if (!strcmp(model->cmd, "quit"))
                return TUI_ACT_QUIT;
            if (!strcmp(model->cmd, "refresh")) {
                memset(model->cmd, 0, sizeof(model->cmd));
                return TUI_ACT_REFRESH;
            }
            if (!strcmp(model->cmd, "view strings")) {
                ui_model_set_view(model, UI_VIEW_STRINGS);
                memset(model->cmd, 0, sizeof(model->cmd));
                return TUI_ACT_NONE;
            }
            if (!strcmp(model->cmd, "view instructions")) {
                ui_model_set_view(model, UI_VIEW_INSTRUCTIONS);
                memset(model->cmd, 0, sizeof(model->cmd));
                return TUI_ACT_NONE;
            }
            if (!strcmp(model->cmd, "view symbols")) {
                ui_model_set_view(model, UI_VIEW_SYMBOLS);
                memset(model->cmd, 0, sizeof(model->cmd));
                return TUI_ACT_NONE;
            }
            if (strstr(model->cmd, "goto ")) {
                char* space = strchr(model->cmd, ' ');
                if (space) {
                    char* address = space + 1;
                    if (model->instructions->length == 0) {
                        snprintf(model->status, sizeof(model->status), "no instructions loaded.");
                        memset(model->cmd, 0, sizeof(model->cmd));
                        return TUI_ACT_NONE;
                    }
                    if (model->view_mode != UI_VIEW_INSTRUCTIONS) {
                        snprintf(model->status, sizeof(model->status), "must be in instructions view to goto address.");
                        memset(model->cmd, 0, sizeof(model->cmd));
                        return TUI_ACT_NONE;
                    }

                    /* parse address. */
                    int base = 10;
                    if (address[0] == '0' && (address[1] == 'x' || address[1] == 'X')) base = 16;
                    char* end = 0x0;
                    unsigned long long addr = strtoull(address, &end, base);
                    if (!end || end == address) {
                        snprintf(model->status, sizeof(model->status), "invalid address: %s", address);
                        memset(model->cmd, 0, sizeof(model->cmd));
                        return TUI_ACT_NONE;
                    }
                    ux_insn_t* first = _get(model->instructions, ux_insn_t*, 0);
                    ux_insn_t* last = _get(model->instructions, ux_insn_t*, model->instructions->length - 1);
                    if (!first || !last) {
                        snprintf(model->status, sizeof(model->status), "no instructions loaded.");
                        memset(model->cmd, 0, sizeof(model->cmd));
                        return TUI_ACT_NONE;
                    }
                    if (addr < (unsigned long long)first->address || addr > (unsigned long long)last->address) {
                        snprintf(model->status, sizeof(model->status), "invalid address: %s", address);
                        memset(model->cmd, 0, sizeof(model->cmd));
                        return TUI_ACT_NONE;
                    }

                    /* find nearest instruction at/after addr. */
                    ssize_t lo = 0;
                    ssize_t hi = (ssize_t)model->instructions->length - 1;
                    ssize_t best = hi;
                    while (lo <= hi) {
                        ssize_t mid = lo + (hi - lo) / 2;
                        ux_insn_t* insn = _get(model->instructions, ux_insn_t*, mid);
                        if (!insn) break;

                        if ((unsigned long long)insn->address >= addr) {
                            best = mid;
                            hi = mid - 1;
                        } else {
                            lo = mid + 1;
                        }
                    }
                    model->selected = best;
                    model->scroll = best;
                    snprintf(model->status, sizeof(model->status), "goto 0x%llx", addr);
                    memset(model->cmd, 0, sizeof(model->cmd));
                    return TUI_ACT_NONE;
                }
            }
            if (strstr(model->cmd, "open ")) {
                /* get the inputted file name. */
                char* space = strchr(model->cmd, ' ');
                if (space) {
                    char* filename = space + 1;

                    /* tell the emitter to load the ENTIRE .text section. */
                    FILE* file = fopen(filename, "rb");
                    if (!file) {
                        snprintf(model->status, sizeof(model->status), \
                            "file could not be found of path: %s", filename);
                        return TUI_ACT_NONE;
                    }
                    fclose(file);

                    /* remove all the old instructions and put in the new. */
                    for (size_t i = 0; i < model->instructions->length; i++) {
                        ux_insn_t* insn = _get(model->instructions, ux_insn_t*, i);
                        free(insn->full_string);
                        free(insn);
                    }
                    dyna_free(model->instructions);
                    model->instructions = dyna_create();

                    /* remove all the old strings and put in the new. */
                    // _foreach(model->strings, char*, str)
                    //     if (str != NULL) free(str);
                    // _endforeach
                    dyna_free(model->strings);
                    model->strings = dyna_create();

                    /* remove all the old symbols. */
                    _foreach(model->symbols, elf_symbol_t*, sym)
                        free(sym);
                    _endforeach
                    dyna_free(model->symbols);
                    model->symbols = dyna_create();

                    /* call the emitter to load the entire section of .text */
                    g_ctx = emit_load(filename, (tup_arch_t){ 0, 0 });
                    emit_scan_text(g_ctx);
                    emit_all(g_ctx, g_wrk_pool);

                    /* extract strings from elf and add to model. */
                    dyna_t* extracted = emit_extract_strings(g_ctx, 4);
                    if (extracted) ui_model_add_strings(model, extracted);
                    dyna_free(extracted);

                    /* extract symbols from elf and add to model. */
                    dyna_t* symbols = emit_extract_symbols(g_ctx);
                    if (extracted) ui_model_add_symbols(model, symbols);

                    /* update status and subtitle. */
                    snprintf(model->status, sizeof(model->status), \
                        "successfully disassembled: %s", filename);

                    /* get the architecture string. */
                    char arch[16u];
                    switch (g_ctx->tuple.arch) {
                        case CS_ARCH_X86: {
                            snprintf(arch, sizeof(arch), "x86");
                            if (g_ctx->tuple.mode == CS_MODE_64)
                                strcat(arch, "_64");
                            break;
                        }
                        case CS_ARCH_AARCH64: {
                            snprintf(arch, sizeof(arch), "aarch64");
                            break;
                        }
                        case CS_ARCH_ARM: {
                            snprintf(arch, sizeof(arch), "arm");
                            break;
                        }
                        default: break;
                    }
                    snprintf(model->subtitle, 128u, \
                        "%s | %s", filename, arch);
                    memset(model->cmd, 0, sizeof(model->cmd));
                    return TUI_ACT_OPEN;
                }
            }
            else {
                /* update status. */
                snprintf(model->status, sizeof(model->status), \
                    "unrecognized command.");
            }

            /* even if nothing was submitted, set the commandline to be 0. */
            memset(model->cmd, 0, sizeof(model->cmd));
            break;
        }
        case KEY_BACKSPACE:
        case 127:
        case 8: {
            /* set the commandline buffer at that position to be 0. */
            if (cmdlen > 0) model->cmd[cmdlen - 1] = '\0';
            return TUI_ACT_NONE;
        }
        default:
            break;
    }

    /* accept printable characters into command bar. */
    if (character >= 32 && character <= 126) {
        if (cmdlen < (int)sizeof(model->cmd) - 1) {
            model->cmd[cmdlen] = (char)character;
            model->cmd[cmdlen + 1] = '\0';
        }
    }
    return TUI_ACT_NONE;
}
