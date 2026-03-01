/**
 * @author Sean Hobeck
 * @date 2026-02-28
 */
#include "emit.h"

/*! @uses disj_post_bytes. */
#include "disj.h"

/*! @uses fprintf, stderr, fopen, fclose, fread, fseek. */
#include <stdio.h>

/*! @uses calloc, malloc, free. */
#include <stdlib.h>

/*! @uses memcmp, memset. */
#include <string.h>

/*! @uses bool, true, false. */
#include <stdbool.h>

/*! @uses internal. */
#include "dyna.h"

/**
 * @brief check if a byte is a common padding byte.
 *
 * @param byte the byte to check.
 * @return if the byte is padding.
 */
internal bool
is_padding(uint8_t byte) {
    return byte == 0x00 || byte == 0x90 || byte == 0xcc;
}

/**
 * @brief check if a sequence of bytes looks like padding.
 *
 * @param data the data to check.
 * @param length the length of the data.
 * @param min_run minimum run of padding bytes to consider it padding.
 * @return if the sequence is padding.
 */
internal bool
is_padding_run(uint8_t* data, size_t length, size_t min_run) {
    size_t count = 0;
    for (size_t i = 0; i < length; i++) {
        if (is_padding(data[i])) count++;
        else count = 0;
        if (count >= min_run) return true;
    }
    return false;
}

/**
 * @brief load an elf binary and prepare it for disassembly.
 *
 * @param path the path to the elf binary.
 * @param tuple the architecture tuple (pass {0, 0} to auto-detect from ELF).
 * @return an emit context if successful, 0x0 o.w.
 */
emit_ctx_t*
emit_load(const char* path, tup_arch_t tuple) {
    /* parse the elf. */
    elf_t* elf = elf_parse(path);
    if (!elf) {
        fprintf(stderr, "lzd, emit_load; could not parse elf file %s.\n", path);
        return 0x0;
    }

    /* if tuple is zero, auto-detect from elf. */
    if (tuple.arch == 0 && tuple.mode == 0) {
        tuple = elf_get_arch(elf);
    }

    /* find the .text section. */
    elf_shdr_t* text_shdr = 0x0;
    _foreach(elf->shdrs, elf_shdr_t*, shdr)
        const char* name = elf_shdr_name(elf, shdr);
        if (name && strcmp(name, ".text") == 0) {
            text_shdr = shdr;
            break;
        }
    _endforeach;
    if (!text_shdr) {
        fprintf(stderr, "lzd, emit_load; could not find .text section.\n");
        elf_free(elf);
        return 0x0;
    }

    /* open the file and read .text section. */
    FILE* file = fopen(path, "rb");
    if (!file) {
        fprintf(stderr, "lzd, emit_load; could not open file %s.\n", path);
        elf_free(elf);
        return 0x0;
    }
    if (fseek(file, text_shdr->offset, SEEK_SET) != 0) {
        fprintf(stderr, "lzd, emit_load; fseek failed.\n");
        fclose(file);
        elf_free(elf);
        return 0x0;
    }

    /* read .text section. */
    uint8_t* text_data = malloc(text_shdr->size);
    if (!text_data) {
        fprintf(stderr, "lzd, emit_load; malloc failed; could not allocate memory for .text.\n");
        fclose(file);
        elf_free(elf);
        return 0x0;
    }
    if (fread(text_data, 1, text_shdr->size, file) != text_shdr->size) {
        fprintf(stderr, "lzd, emit_load; fread failed; could not read .text section.\n");
        free(text_data);
        fclose(file);
        elf_free(elf);
        return 0x0;
    }
    fclose(file);

    /* allocate and initialize context. */
    emit_ctx_t* ctx = calloc(1u, sizeof *ctx);
    ctx->elf = elf;
    ctx->tuple = tuple;
    ctx->text_data = text_data;
    ctx->text_vaddr = text_shdr->addr;
    ctx->text_size = text_shdr->size;
    ctx->code_ranges = dyna_create();
    return ctx;
}

/**
 * @brief free an emit context.
 *
 * @param ctx the context to be freed.
 */
void
emit_free(emit_ctx_t* ctx) {
    if (!ctx) return;
    elf_free(ctx->elf);
    free(ctx->text_data);
    if (ctx->code_ranges) {
        _foreach(ctx->code_ranges, code_range_t*, range)
            free(range);
        _endforeach;
        dyna_free(ctx->code_ranges);
    }
    free(ctx);
}

/**
 * @brief scan .text section and identify code ranges (skip nops/padding).
 *
 * @param ctx the emit context.
 * @return -1 if a failure occurs, 0 o.w.
 */
ssize_t
emit_scan_text(emit_ctx_t* ctx) {
    if (!ctx || !ctx->text_data) return -1;

    /* scan through .text and find contiguous code ranges. */
    size_t i = 0;
    while (i < ctx->text_size) {
        /* skip padding. */
        if (is_padding(ctx->text_data[i])) {
            i++;
            continue;
        }

        /* found start of code, find the end. */
        size_t start = i;
        while (i < ctx->text_size) {
            /* look ahead for padding runs (at least 16 bytes). */
            if (i + 16 <= ctx->text_size && is_padding_run(&ctx->text_data[i], 16, 16)) {
                break;
            }
            i++;
        }

        /* create a code range. */
        size_t length = i - start;
        if (length > 0) {
            code_range_t* range = calloc(1u, sizeof *range);
            range->vaddr = ctx->text_vaddr + start;
            range->offset = start;
            range->length = length;
            dyna_push(ctx->code_ranges, range);
        }
    }
    return 0;
}

/**
 * @brief emit disassembly jobs for a specific virtual address range.
 *
 * @param ctx the emit context.
 * @param pool the worker pool to post jobs to.
 * @param vaddr_start the starting virtual address.
 * @param vaddr_end the ending virtual address.
 * @return -1 if a failure occurs, 0 o.w.
 */
ssize_t
emit_range(emit_ctx_t* ctx, wrk_pool_t* pool, uint64_t vaddr_start, uint64_t vaddr_end) {
    if (!ctx || !pool) return -1;

    /* find code ranges that intersect with the requested range. */
    size_t posted = 0;
    _foreach(ctx->code_ranges, code_range_t*, range)
        uint64_t range_end = range->vaddr + range->length;

        /* check for intersection. */
        if (range->vaddr >= vaddr_end || range_end <= vaddr_start)
            continue;

        /* calculate the intersection. */
        uint64_t job_vaddr = range->vaddr > vaddr_start ? range->vaddr : vaddr_start;
        uint64_t job_end = range_end < vaddr_end ? range_end : vaddr_end;
        size_t job_offset = (job_vaddr - ctx->text_vaddr);
        size_t job_length = job_end - job_vaddr;

        /* post the disassembly job. */
        if (disj_post_bytes(pool, ctx->tuple, ctx->text_data + job_offset, \
            job_length, job_vaddr) != 0) {
            fprintf(stderr, "lzd, emit_range; disj_post_bytes failed.\n");
            return -1;
        }
        posted++;
    _endforeach;
    return posted > 0 ? 0 : -1;
}

/**
 * @brief emit all disassembly jobs for the entire binary.
 *
 * @param ctx the emit context.
 * @param pool the worker pool to post jobs to.
 * @return -1 if a failure occurs, 0 o.w.
 */
ssize_t
emit_all(emit_ctx_t* ctx, wrk_pool_t* pool) {
    if (!ctx || !pool) return -1;

    /* post all code ranges. */
    _foreach(ctx->code_ranges, code_range_t*, range)
        if (disj_post_bytes(pool, ctx->tuple, ctx->text_data + range->offset, \
            range->length, range->vaddr) != 0) {
            fprintf(stderr, "lzd, emit_all; disj_post_bytes failed.\n");
            return -1;
        }
    _endforeach;
    return 0;
}

/**
 * @brief check if a byte is printable ascii.
 *
 * @param c the byte to check.
 * @return if the byte is printable.
 */
internal bool
is_printable(uint8_t c) {
    return c >= 0x20 && c <= 0x7e;
}

/**
 * @brief check if string is valid (has enough alphanumeric chars).
 *
 * @param str the string to check.
 * @param len the length of the string.
 * @return if the string appears valid.
 */
internal bool
is_valid_string(const char* str, size_t len) {
    if (len == 0) return false;

    size_t alnum_count = 0;
    size_t space_count = 0;

    for (size_t i = 0; i < len; i++) {
        uint8_t c = str[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))
            alnum_count++;
        else if (c == ' ')
            space_count++;
    }

    /* require at least 50% alphanumeric, and not all spaces. */
    return (alnum_count * 2 >= len) && (space_count < len);
}

/**
 * @brief extract printable strings from elf using strings command.
 *
 * @param ctx the emit context.
 * @param min_len minimum string length to extract (default 4).
 * @return dyna_t* of char* strings, or 0x0 on failure.
 */
dyna_t*
emit_extract_strings(emit_ctx_t* ctx, size_t min_len) {
    dyna_t* strings = dyna_create();
    if (!strings) return 0x0;

    /* scan .rodata and .data sections. */
    const char* section_names[] = { ".rodata", ".data", ".dynstr", ".strtab" };
    _foreach(ctx->elf->shdrs, elf_shdr_t*, header)
        /* find the section. */
        bool found = false;
        const char* name = elf_shdr_name(ctx->elf, header);
        if (!name) continue;
        for (size_t j = 0; j < 4; j++) {
            if (!strcmp(name, section_names[j])) {
                found = true;
                break;
            }
        }
        if (!header || header->size == 0 || !found) continue;

        /* read section data from file. */
        FILE* f = fopen(ctx->elf->path, "rb");
        if (!f) continue;
        uint8_t* data = malloc(header->size);
        if (!data) {
            fclose(f);
            continue;
        }
        fseek(f, header->offset, SEEK_SET);
        size_t read = fread(data, 1, header->size, f);
        fclose(f);
        if (read != header->size) {
            free(data);
            continue;
        }

        /* extract strings. */
        size_t str_start = 0;
        bool in_string = false;
        for (size_t j = 0; j < header->size; j++) {
            if (is_printable(data[j]) && data[j] != 0) {
                if (!in_string) {
                    str_start = j;
                    in_string = true;
                }
            } else {
                if (in_string) {
                    size_t len = j - str_start;
                    if (len >= min_len) {
                        /* validate the string before adding. */
                        if (is_valid_string((char*)(data + str_start), len)) {
                            char* str = calloc(1, len + 1);
                            if (str) {
                                memcpy(str, data + str_start, len);
                                dyna_push(strings, str);
                            }
                        }
                    }
                    in_string = false;
                }
            }
        }
        free(data);
    _endforeach;
    return strings;
}

/* ... */
typedef struct {
    uint32_t name; /* string table index. */
    uint32_t value; /* value. */
    uint32_t size; /* size. */
    uint8_t info; /* type and binding. */
    uint8_t other; /* visibility. */
    uint16_t shndx; /* section index. */
} elf32_sym_t;

/* ... */
typedef struct {
    uint32_t name; /* string table index. */
    uint8_t info; /* type and binding. */
    uint8_t other; /* visibility. */
    uint16_t shndx; /* section index. */
    uint64_t value; /* value. */
    uint64_t size; /* size. */
} elf64_sym_t;

/* st_info decoding. */
#define ELF_ST_BIND(i) ((uint8_t)((i) >> 4))
#define ELF_ST_TYPE(i) ((uint8_t)((i) & 0x0f))

/**
 * @brief check if a string offset is valid and nul-terminated.
 *
 * @param strtab the string table bytes.
 * @param strtab_size size of string table.
 * @param off offset into string table.
 * @return if the string is valid.
 */
internal bool
is_valid_strtab_off(uint8_t* strtab, size_t strtab_size, uint32_t off) {
    if (!strtab || strtab_size == 0) return false;
    if (off >= strtab_size) return false;
    /* must be nul-terminated within the table. */
    return memchr(strtab + off, 0, strtab_size - off) != 0x0;
}

/**
 * @brief extract symbols from elf symbol tables.
 *
 * @param ctx the emit context.
 * @return dyna_t* of elf_symbol_t* symbols, or 0x0 on failure.
 */
dyna_t*
emit_extract_symbols(emit_ctx_t* ctx) {
    if (!ctx || !ctx->elf) return 0x0;

    dyna_t* symbols = dyna_create();
    if (!symbols) return 0x0;

    /* scan .symtab and .dynsym sections. */
    const char* section_names[] = { ".symtab", ".dynsym" };
    _foreach(ctx->elf->shdrs, elf_shdr_t*, symhdr)
        /* find the section. */
        bool found = false;
        const char* name = elf_shdr_name(ctx->elf, symhdr);
        if (!name) continue;
        for (size_t j = 0; j < 2; j++) {
            if (!strcmp(name, section_names[j])) {
                found = true;
                break;
            }
        }
        if (!symhdr || symhdr->size == 0 || !found) continue;

        /* resolve associated string table using sh_link. */
        if (symhdr->link >= ctx->elf->shdrs->length) continue;
        elf_shdr_t* strhdr = _get(ctx->elf->shdrs, elf_shdr_t*, symhdr->link);
        if (!strhdr || strhdr->size == 0) continue;
        if (strhdr->type != ELF_SHT_STRTAB) continue;

        /* read symtab and strtab data from file. */
        FILE* f = fopen(ctx->elf->path, "rb");
        if (!f) continue;

        uint8_t* sym_data = malloc(symhdr->size);
        if (!sym_data) {
            fclose(f);
            continue;
        }
        uint8_t* str_data = malloc(strhdr->size);
        if (!str_data) {
            free(sym_data);
            fclose(f);
            continue;
        }

        /* read sym section. */
        fseek(f, symhdr->offset, SEEK_SET);
        size_t sym_read = fread(sym_data, 1, symhdr->size, f);
        if (sym_read != symhdr->size) {
            free(sym_data);
            free(str_data);
            fclose(f);
            continue;
        }

        /* read str section. */
        fseek(f, strhdr->offset, SEEK_SET);
        size_t str_read = fread(str_data, 1, strhdr->size, f);
        fclose(f);
        if (str_read != strhdr->size) {
            free(sym_data);
            free(str_data);
            continue;
        }

        /* parse symbols. */
        if (ctx->elf->class == ELF_CLASS_32) {
            size_t entsize = symhdr->entsize ? symhdr->entsize : sizeof(elf32_sym_t);
            if (entsize == 0) {
                free(sym_data);
                free(str_data);
                continue;
            }
            size_t count = symhdr->size / entsize;
            for (size_t i = 0; i < count; i++) {
                elf32_sym_t* s = (elf32_sym_t*)(sym_data + i * entsize);
                if (s->name == 0) continue;
                if (!is_valid_strtab_off(str_data, strhdr->size, s->name)) continue;
                const char* sym_name = (const char*)(str_data + s->name);
                if (!sym_name || !*sym_name) continue;

                /* copy name. */
                size_t len = strnlen(sym_name, strhdr->size - s->name);
                char* owned = calloc(1, len + 1);
                if (!owned) continue;
                memcpy(owned, sym_name, len);
                elf_symbol_t* out = calloc(1u, sizeof *out);
                if (!out) {
                    free(owned);
                    continue;
                }
                out->name = owned;
                out->value = (uint64_t)s->value;
                out->size = (uint64_t)s->size;
                out->info = s->info;
                out->other = s->other;
                out->shndx = s->shndx;
                out->bind = ELF_ST_BIND(s->info);
                out->type = ELF_ST_TYPE(s->info);
                dyna_push(symbols, out);
            }
        } else if (ctx->elf->class == ELF_CLASS_64) {
            size_t entsize = symhdr->entsize ? symhdr->entsize : sizeof(elf64_sym_t);
            if (entsize == 0) {
                free(sym_data);
                free(str_data);
                continue;
            }
            size_t count = symhdr->size / entsize;
            for (size_t i = 0; i < count; i++) {
                elf64_sym_t* s = (elf64_sym_t*)(sym_data + i * entsize);
                if (s->name == 0) continue;
                if (!is_valid_strtab_off(str_data, strhdr->size, s->name)) continue;
                const char* sym_name = (const char*)(str_data + s->name);
                if (!sym_name || !*sym_name) continue;

                /* copy name. */
                size_t len = strnlen(sym_name, strhdr->size - s->name);
                char* owned = calloc(1, len + 1);
                if (!owned) continue;
                memcpy(owned, sym_name, len);
                elf_symbol_t* out = calloc(1u, sizeof *out);
                if (!out) {
                    free(owned);
                    continue;
                }
                out->name = owned;
                out->value = s->value;
                out->size = s->size;
                out->info = s->info;
                out->other = s->other;
                out->shndx = s->shndx;
                out->bind = ELF_ST_BIND(s->info);
                out->type = ELF_ST_TYPE(s->info);
                dyna_push(symbols, out);
            }
        }
        free(sym_data);
        free(str_data);
    _endforeach;
    return symbols;
}