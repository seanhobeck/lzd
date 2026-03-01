/**
 * @author Sean Hobeck
 * @date 2026-03-01
 */
#include "ui.h"

/*! @uses ux_insn_t, ux_handle_key. */
#include "ux.h"

/*! @uses ncurses. */
#include <ncurses.h>

/*! @uses calloc, free. */
#include <stdlib.h>

/*! @uses strncpy, strnlen. */
#include <string.h>

/*! @uses fprintf, stderr. */
#include <stdio.h>

/*! @uses internal. */
#include "dyna.h"

/*! @uses elf_symbol_t. */
#include "elfx.h"

/**
 * @brief clamp an integer to a range.
 *
 * @param v the value to clamp.
 * @param lo the lower bound.
 * @param hi the upper bound.
 * @return the clamped value.
 */
internal int
clampi(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

/**
 * @brief draw a box around a window.
 *
 * @param w the window.
 */
internal void
draw_boxed(WINDOW* w) {
    box(w, 0, 0);
}

/**
 * @brief draw the header section of the ui.
 *
 * @param w the window.
 * @param m the ui model.
 */
internal void
draw_header(WINDOW* w, ui_model_t* m) {
    werase(w);
    draw_boxed(w);
    int h, wd;
    getmaxyx(w, h, wd);
    (void)h;

    /* title. */
    if (m->title) {
        wattron(w, A_BOLD);
        mvwprintw(w, 0, 2, " %s ", m->title);
        wattroff(w, A_BOLD);
    }

    /* subtitle line inside box. */
    if (m->subtitle) {
        mvwprintw(w, 1, 2, "%.*s", wd - 4, m->subtitle);
    }
    wrefresh(w);
}

/**
 * @brief draw the instruction list section of the ui.
 *
 * @param w the window.
 * @param m the ui model.
 */
internal void
draw_list(WINDOW* w, ui_model_t* m) {
    werase(w);
    draw_boxed(w);

    int h, wd;
    getmaxyx(w, h, wd);
    int inner_h = h - 2;
    int inner_w = wd - 2;

    /* lock for thread safety. */
    pthread_mutex_lock(&m->lock);

    /* determine what to display based on view mode. */
    dyna_t* items = (m->view_mode == UI_VIEW_STRINGS) ? m->strings : m->view_mode == UI_VIEW_SYMBOLS ? \
        m->symbols : m->instructions;
    ssize_t item_count = items ? items->length : 0;
    const char* view_name = (m->view_mode == UI_VIEW_STRINGS) ? "strings" : \
        m->view_mode == UI_VIEW_SYMBOLS ? "symbols" : "instructions";

    /* keep scroll/selected sane. */
    m->selected = clampi((int)m->selected, 0, (int)(item_count > 0 ? item_count - 1 : 0));
    m->scroll = clampi((int)m->scroll, 0, (int)(item_count > inner_h ? item_count - inner_h : 0));

    /* ensure selected is visible. */
    if (m->selected < m->scroll) m->scroll = m->selected;
    if (m->selected >= m->scroll + inner_h) m->scroll = m->selected - inner_h + 1;

    /* header label. */
    mvwprintw(w, 0, 2, " %s (%zd) ", view_name, item_count);

    /* draw visible items. */
    for (int row = 0; row < inner_h; row++) {
        ssize_t idx = m->scroll + row;
        if (idx >= item_count) break;

        int sel = (idx == m->selected);
        if (sel) wattron(w, A_REVERSE);

        /* get item based on view mode. */
        const char* s = "";
        if (m->view_mode == UI_VIEW_STRINGS || m->view_mode == UI_VIEW_SYMBOLS) {
            char* str = _get(items, char*, idx);
            s = str ? str : "";
        } else {
            ux_insn_t* insn = _get(items, ux_insn_t*, idx);
            s = (insn && insn->full_string) ? insn->full_string : "";
        }
        mvwprintw(w, 1 + row, 1, " %.*s", inner_w - 2, s);

        if (sel) wattroff(w, A_REVERSE);
    }

    /* scrollbar-ish marker. */
    if (item_count > inner_h) {
        int bar_h = inner_h;
        int pos = (int)((long long)m->scroll * bar_h / (item_count - inner_h));
        pos = clampi(pos, 0, bar_h - 1);
        mvwaddch(w, 1 + pos, wd - 2, ACS_CKBOARD);
    }

    pthread_mutex_unlock(&m->lock);
    wrefresh(w);
}

/**
 * @brief draw the footer section of the ui.
 *
 * @param w the window.
 * @param m the ui model.
 */
internal void
draw_footer(WINDOW* w, ui_model_t* m) {
    werase(w);
    draw_boxed(w);

    int h, wd;
    getmaxyx(w, h, wd);
    (void)h;

    /* status (top line). */
    if (m->status[0]) {
        mvwprintw(w, 1, 2, "%.*s", wd - 4, m->status);
    } else {
        mvwprintw(w, 1, 2, "%.*s", wd - 4, "\'open ./binary\'  quit  refresh  arrows=move  \'view" \
            " strings\'");
    }

    /* command bar (bottom line inside footer). */
    wattron(w, A_BOLD);
    mvwprintw(w, 2, 2, ":%.*s", wd - 5, m->cmd);
    wattroff(w, A_BOLD);

    /* put cursor at end of cmd (within bounds). */
    int curx = 3 + (int)strnlen(m->cmd, sizeof(m->cmd) - 1);
    curx = clampi(curx, 3, wd - 2);
    wmove(w, 2, curx);
    wrefresh(w);
}

/**
 * @brief calculate layout dimensions.
 *
 * @param term_h terminal height.
 * @param term_w terminal width.
 * @param hdr_h output header height.
 * @param ftr_h output footer height.
 * @param lst_h output list height.
 */
internal void
layout(int term_h, int term_w, int* hdr_h, int* ftr_h, int* lst_h) {
    (void)term_w;
    *hdr_h = 3;
    *ftr_h = 4;
    *lst_h = term_h - *hdr_h - *ftr_h;
    if (*lst_h < 3) *lst_h = 3;
}

/**
 * @brief create a new ui model.
 *
 * @param title the title string (will be copied).
 * @param subtitle the subtitle string (will be copied).
 * @return a pointer to an allocated ui model if successful, 0x0 o.w.
 */
ui_model_t*
ui_model_create(const char* title, const char* subtitle) {
    ui_model_t* model = calloc(1u, sizeof *model);
    if (!model) {
        fprintf(stderr, "lzd, ui_model_create; calloc failed; could not allocate memory for model.\n");
        return 0x0;
    }

    /* copy title and subtitle. */
    if (title) {
        model->title = calloc(1u, strlen(title) + 1);
        strncpy(model->title, title, strlen(title));
    }
    if (subtitle) {
        size_t n = strlen(subtitle);
        if (n > 256u) n = 256u;
        model->subtitle = calloc(1u, 256u + 1u);
        strncpy(model->subtitle, subtitle, n);
    }

    /* initialize dynamic arrays. */
    model->instructions = dyna_create();
    model->strings = dyna_create();
    model->symbols = dyna_create();
    model->view_mode = UI_VIEW_INSTRUCTIONS;
    pthread_mutex_init(&model->lock, 0x0);
    return model;
}

/**
 * @brief free a ui model.
 *
 * @param model the model to be freed.
 */
void
ui_model_free(ui_model_t* model) {
    if (!model) return;

    free(model->title);
    free(model->subtitle);

    /* free all instructions and their strings. */
    if (model->instructions) {
        _foreach(model->instructions, ux_insn_t*, insn)
            free(insn->full_string);
            free(insn);
        _endforeach;
        dyna_free(model->instructions);
    }

    /* free all strings. */
    if (model->strings) {
        dyna_free(model->strings);
    }
    if (model->symbols) {
        dyna_free(model->symbols);
    }
    pthread_mutex_destroy(&model->lock);
    free(model);
}

/**
 * @brief add instructions to the ui model.
 *
 * @param model the ui model.
 * @param insns dynamic array of ux_insn_t*.
 */
void
ui_model_add_insns(ui_model_t* model, dyna_t* insns) {
    if (!model || !insns) return;
    pthread_mutex_lock(&model->lock);
    _foreach(insns, ux_insn_t*, insn)
        dyna_push(model->instructions, insn);
    _endforeach;
    pthread_mutex_unlock(&model->lock);
}

/**
 * @brief clear all instructions from the ui model.
 *
 * @param model the ui model.
 */
void
ui_model_clear(ui_model_t* model) {
    if (!model) return;
    pthread_mutex_lock(&model->lock);
    if (model->instructions) {
        _foreach(model->instructions, ux_insn_t*, insn)
            free(insn->full_string);
            free(insn);
        _endforeach;
        dyna_free(model->instructions);
        model->instructions = dyna_create();
    }
    model->selected = 0;
    model->scroll = 0;
    pthread_mutex_unlock(&model->lock);
}

/**
 * @brief add strings to the ui model.
 *
 * @param model the ui model.
 * @param strings dynamic array of char* strings.
 */
void
ui_model_add_strings(ui_model_t* model, dyna_t* strings) {
    if (!model || !strings) return;

    pthread_mutex_lock(&model->lock);
    _foreach(strings, char*, str)
        dyna_push(model->strings, str);
    _endforeach;
    pthread_mutex_unlock(&model->lock);
}

/**
 * @brief add elf symbol strings to the ui model.
 *
 * @param model the ui model.
 * @param symbols dynamic array of elf_symbol_t*.
 */
void
ui_model_add_symbols(ui_model_t* model, dyna_t* symbols) {
    if (!model || !symbols) return;

    pthread_mutex_lock(&model->lock);
    _foreach(symbols, elf_symbol_t*, sym)
        char* string = calloc(1u, 256u + 1u);
        if (sym->value) snprintf(string, 256u, "%p:\t%s", (void*) (sym->value), sym->name);
        else snprintf(string, 256u, "(lib./ext.):\t%s", sym->name);
        dyna_push(model->symbols, string);
    _endforeach;
    pthread_mutex_unlock(&model->lock);
};

/**
 * @brief set the view mode.
 *
 * @param model the ui model.
 * @param mode the new view mode.
 */
void
ui_model_set_view(ui_model_t* model, ui_view_mode_t mode) {
    if (!model) return;

    pthread_mutex_lock(&model->lock);
    model->view_mode = mode;
    model->selected = 0;
    model->scroll = 0;
    snprintf(model->status, sizeof(model->status), "switched to %s view", \
             mode == UI_VIEW_STRINGS ? "strings" : mode == UI_VIEW_SYMBOLS ? \
             "symbols" : "instructions");
    pthread_mutex_unlock(&model->lock);
}

/**
 * @brief run the ui event loop (blocking).
 *
 * @param model the ui model.
 * @return the action that caused the loop to exit.
 */
ui_act_t
ui_run(ui_model_t* model) {
    /* initialize everything. */
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(1);
    nodelay(stdscr, FALSE);
    ui_act_t last_act = TUI_ACT_NONE;

    /* initial draw before loop. */
    {
        int term_h, term_w;
        getmaxyx(stdscr, term_h, term_w);
        int hdr_h, ftr_h, lst_h;
        layout(term_h, term_w, &hdr_h, &ftr_h, &lst_h);

        WINDOW* hdr = newwin(hdr_h, term_w, 0, 0);
        WINDOW* lst = newwin(lst_h, term_w, hdr_h, 0);
        WINDOW* ftr = newwin(ftr_h, term_w, hdr_h + lst_h, 0);

        draw_header(hdr, model);
        draw_list(lst, model);
        draw_footer(ftr, model);
        doupdate();

        delwin(hdr);
        delwin(lst);
        delwin(ftr);
    }

    while (1) {
        int term_h, term_w;
        getmaxyx(stdscr, term_h, term_w);

        /* get the layout of the terminal. */
        int hdr_h, ftr_h, lst_h;
        layout(term_h, term_w, &hdr_h, &ftr_h, &lst_h);

        /* create new windows. */
        WINDOW* hdr = newwin(hdr_h, term_w, 0, 0);
        WINDOW* lst = newwin(lst_h, term_w, hdr_h, 0);
        WINDOW* ftr = newwin(ftr_h, term_w, hdr_h + lst_h, 0);

        /* draw everything. */
        draw_header(hdr, model);
        draw_list(lst, model);
        draw_footer(ftr, model);
        doupdate();

        /* ux handler. */
        int ch = getch();
        if (ch == KEY_RESIZE) {
            /* just loop and redraw. */
            last_act = TUI_ACT_NONE;
        } else {
            last_act = ux_handle_key(model, ch);
        }

        /* delete each window for a new render cycle. */
        delwin(hdr);
        delwin(lst);
        delwin(ftr);
        if (last_act == TUI_ACT_QUIT) break;
    }
    endwin();
    return last_act;
}
