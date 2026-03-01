/**
 * @author Sean Hobeck
 * @date 2026-03-01
 */
#include "elfx.h"

/*! @uses fprintf, stderr, fopen, fclose, fread, fseek, ftell. */
#include <stdio.h>

/*! @uses calloc, malloc, free. */
#include <stdlib.h>

/*! @uses memcpy. */
#include <string.h>

/*! @uses internal. */
#include "dyna.h"

/* elf magic number. */
#define ELF_MAGIC 0x464c457fu /* 0x7f 'E' 'L' 'F' */

/* elf identification indices. */
#define EI_MAG0 0u      /* magic number byte 0. */
#define EI_MAG1 1u      /* magic number byte 1. */
#define EI_MAG2 2u      /* magic number byte 2. */
#define EI_MAG3 3u      /* magic number byte 3. */
#define EI_CLASS 4u     /* file class. */
#define EI_DATA 5u      /* data encoding. */
#define EI_VERSION 6u   /* file version. */
#define EI_NIDENT 16u   /* size of e_ident[]. */

/* elf32 header. */
typedef struct {
    uint8_t e_ident[EI_NIDENT];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint32_t e_entry;
    uint32_t e_phoff;
    uint32_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} __attribute__((packed)) elf32_ehdr_t;

/* elf64 header. */
typedef struct {
    uint8_t e_ident[EI_NIDENT];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} __attribute__((packed)) elf64_ehdr_t;

/* elf32 program header. */
typedef struct {
    uint32_t p_type;
    uint32_t p_offset;
    uint32_t p_vaddr;
    uint32_t p_paddr;
    uint32_t p_filesz;
    uint32_t p_memsz;
    uint32_t p_flags;
    uint32_t p_align;
} __attribute__((packed)) elf32_phdr_t;

/* elf64 program header. */
typedef struct {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} __attribute__((packed)) elf64_phdr_t;

/* elf32 section header. */
typedef struct {
    uint32_t sh_name;
    uint32_t sh_type;
    uint32_t sh_flags;
    uint32_t sh_addr;
    uint32_t sh_offset;
    uint32_t sh_size;
    uint32_t sh_link;
    uint32_t sh_info;
    uint32_t sh_addralign;
    uint32_t sh_entsize;
} __attribute__((packed)) elf32_shdr_t;

/* elf64 section header. */
typedef struct {
    uint32_t sh_name;
    uint32_t sh_type;
    uint64_t sh_flags;
    uint64_t sh_addr;
    uint64_t sh_offset;
    uint64_t sh_size;
    uint32_t sh_link;
    uint32_t sh_info;
    uint64_t sh_addralign;
    uint64_t sh_entsize;
} __attribute__((packed)) elf64_shdr_t;

/**
 * @brief check if an elf identifier is valid.
 *
 * @param ident the elf identifier.
 * @return if the identifier is valid.
 */
internal bool
elf_valid_ident(uint8_t* ident) {
    return ident[EI_MAG0] == 0x7fu && ident[EI_MAG1] == 'E' &&
           ident[EI_MAG2] == 'L' && ident[EI_MAG3] == 'F';
}

/**
 * @brief parse an elf32 file from a buffer.
 *
 * @param buffer the buffer containing the elf file.
 * @param size the size of the buffer.
 * @return a pointer to an allocated elf_t if successful, 0x0 o.w.
 */
internal elf_t*
elf_parse32(uint8_t* buffer, size_t size) {
    elf32_ehdr_t* ehdr = (elf32_ehdr_t*)buffer;
    if (size < sizeof *ehdr) {
        fprintf(stderr, "lzd, elf_parse32; buffer too small for elf32 header.\n");
        return 0x0;
    }

    /* allocate our elf structure. */
    elf_t* elf = calloc(1u, sizeof *elf);
    elf->class = ELF_CLASS_32;
    elf->data = ehdr->e_ident[EI_DATA];
    elf->type = ehdr->e_type;
    elf->machine = ehdr->e_machine;
    elf->entry = ehdr->e_entry;
    elf->phoff = ehdr->e_phoff;
    elf->shoff = ehdr->e_shoff;
    elf->phnum = ehdr->e_phnum;
    elf->shnum = ehdr->e_shnum;
    elf->shstrndx = ehdr->e_shstrndx;
    elf->phdrs = dyna_create();
    elf->shdrs = dyna_create();

    /* parse program headers. */
    if (elf->phnum > 0 && elf->phoff + elf->phnum * sizeof(elf32_phdr_t) <= size) {
        for (uint16_t i = 0; i < elf->phnum; i++) {
            elf32_phdr_t* phdr32 = (elf32_phdr_t*)(buffer + elf->phoff + i * sizeof(elf32_phdr_t));
            elf_phdr_t* phdr = calloc(1u, sizeof *phdr);
            phdr->type = phdr32->p_type;
            phdr->flags = phdr32->p_flags;
            phdr->offset = phdr32->p_offset;
            phdr->vaddr = phdr32->p_vaddr;
            phdr->paddr = phdr32->p_paddr;
            phdr->filesz = phdr32->p_filesz;
            phdr->memsz = phdr32->p_memsz;
            phdr->align = phdr32->p_align;
            dyna_push(elf->phdrs, phdr);
        }
    }

    /* parse section headers. */
    if (elf->shnum > 0 && elf->shoff + elf->shnum * sizeof(elf32_shdr_t) <= size) {
        for (uint16_t i = 0; i < elf->shnum; i++) {
            elf32_shdr_t* shdr32 = (elf32_shdr_t*)(buffer + elf->shoff + i * sizeof(elf32_shdr_t));
            elf_shdr_t* shdr = calloc(1u, sizeof *shdr);
            shdr->name = shdr32->sh_name;
            shdr->type = shdr32->sh_type;
            shdr->flags = shdr32->sh_flags;
            shdr->addr = shdr32->sh_addr;
            shdr->offset = shdr32->sh_offset;
            shdr->size = shdr32->sh_size;
            shdr->link = shdr32->sh_link;
            shdr->info = shdr32->sh_info;
            shdr->addralign = shdr32->sh_addralign;
            shdr->entsize = shdr32->sh_entsize;
            dyna_push(elf->shdrs, shdr);
        }

        /* parse section header string table. */
        if (elf->shstrndx < elf->shnum) {
            elf_shdr_t* shstrtab = _get(elf->shdrs, elf_shdr_t*, elf->shstrndx);
            if (shstrtab && shstrtab->offset + shstrtab->size <= size) {
                elf->shstrtab_size = shstrtab->size;
                elf->shstrtab = malloc(elf->shstrtab_size);
                memcpy(elf->shstrtab, buffer + shstrtab->offset, elf->shstrtab_size);
            }
        }
    }
    return elf;
}

/**
 * @brief parse an elf64 file from a buffer.
 *
 * @param buffer the buffer containing the elf file.
 * @param size the size of the buffer.
 * @return a pointer to an allocated elf_t if successful, 0x0 o.w.
 */
internal elf_t*
elf_parse64(uint8_t* buffer, size_t size) {
    elf64_ehdr_t* ehdr = (elf64_ehdr_t*)buffer;
    if (size < sizeof *ehdr) {
        fprintf(stderr, "lzd, elf_parse64; buffer too small for elf64 header.\n");
        return 0x0;
    }

    /* allocate our elf structure. */
    elf_t* elf = calloc(1u, sizeof *elf);
    elf->class = ELF_CLASS_64;
    elf->data = ehdr->e_ident[EI_DATA];
    elf->type = ehdr->e_type;
    elf->machine = ehdr->e_machine;
    elf->entry = ehdr->e_entry;
    elf->phoff = ehdr->e_phoff;
    elf->shoff = ehdr->e_shoff;
    elf->phnum = ehdr->e_phnum;
    elf->shnum = ehdr->e_shnum;
    elf->shstrndx = ehdr->e_shstrndx;
    elf->phdrs = dyna_create();
    elf->shdrs = dyna_create();

    /* parse program headers. */
    if (elf->phnum > 0 && elf->phoff + elf->phnum * sizeof(elf64_phdr_t) <= size) {
        for (uint16_t i = 0; i < elf->phnum; i++) {
            elf64_phdr_t* phdr64 = (elf64_phdr_t*)(buffer + elf->phoff + i * sizeof(elf64_phdr_t));
            elf_phdr_t* phdr = calloc(1u, sizeof *phdr);
            phdr->type = phdr64->p_type;
            phdr->flags = phdr64->p_flags;
            phdr->offset = phdr64->p_offset;
            phdr->vaddr = phdr64->p_vaddr;
            phdr->paddr = phdr64->p_paddr;
            phdr->filesz = phdr64->p_filesz;
            phdr->memsz = phdr64->p_memsz;
            phdr->align = phdr64->p_align;
            dyna_push(elf->phdrs, phdr);
        }
    }

    /* parse section headers. */
    if (elf->shnum > 0 && elf->shoff + elf->shnum * sizeof(elf64_shdr_t) <= size) {
        for (uint16_t i = 0; i < elf->shnum; i++) {
            elf64_shdr_t* shdr64 = (elf64_shdr_t*)(buffer + elf->shoff + i * sizeof(elf64_shdr_t));
            elf_shdr_t* shdr = calloc(1u, sizeof *shdr);
            shdr->name = shdr64->sh_name;
            shdr->type = shdr64->sh_type;
            shdr->flags = shdr64->sh_flags;
            shdr->addr = shdr64->sh_addr;
            shdr->offset = shdr64->sh_offset;
            shdr->size = shdr64->sh_size;
            shdr->link = shdr64->sh_link;
            shdr->info = shdr64->sh_info;
            shdr->addralign = shdr64->sh_addralign;
            shdr->entsize = shdr64->sh_entsize;
            dyna_push(elf->shdrs, shdr);
        }

        /* parse section header string table. */
        if (elf->shstrndx < elf->shnum) {
            elf_shdr_t* shstrtab = _get(elf->shdrs, elf_shdr_t*, elf->shstrndx);
            if (shstrtab && shstrtab->offset + shstrtab->size <= size) {
                elf->shstrtab_size = shstrtab->size;
                elf->shstrtab = malloc(elf->shstrtab_size);
                memcpy(elf->shstrtab, buffer + shstrtab->offset, elf->shstrtab_size);
            }
        }
    }
    return elf;
}

/**
 * @brief parse an elf file from a file path.
 *
 * @param path the path to the elf file.
 * @return a pointer to an allocated elf_t if successful, 0x0 o.w.
 */
elf_t*
elf_parse(const char* path) {
    /* open the file. */
    FILE* file = fopen(path, "rb");
    if (!file) {
        fprintf(stderr, "lzd, elf_parse; could not open file %s.\n", path);
        return 0x0;
    }

    /* get the file size. */
    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    fseek(file, 0, SEEK_SET);

    /* read the file into a buffer. */
    uint8_t* buffer = malloc(size);
    if (!buffer) {
        fprintf(stderr, "lzd, elf_parse; malloc failed; could not allocate memory for buffer.\n");
        fclose(file);
        return 0x0;
    }
    if (fread(buffer, 1u, size, file) != size) {
        fprintf(stderr, "lzd, elf_parse; fread failed; could not read file %s.\n", path);
        free(buffer);
        fclose(file);
        return 0x0;
    }
    fclose(file);

    /* check if the elf is valid. */
    if (size < EI_NIDENT || !elf_valid_ident(buffer)) {
        fprintf(stderr, "lzd, elf_parse; invalid elf file %s.\n", path);
        free(buffer);
        return 0x0;
    }

    /* parse the elf based on class. */
    elf_t* elf = 0x0;
    if (buffer[EI_CLASS] == ELF_CLASS_32)
        elf = elf_parse32(buffer, size);
    else if (buffer[EI_CLASS] == ELF_CLASS_64)
        elf = elf_parse64(buffer, size);
    else
        fprintf(stderr, "lzd, elf_parse; unsupported elf class %d.\n", buffer[EI_CLASS]);

    /* store path in elf structure. */
    if (elf) {
        elf->path = strdup(path);
    }

    free(buffer);
    return elf;
}

/**
 * @brief free an elf structure and all of its data.
 *
 * @param elf the elf structure to be freed.
 */
void
elf_free(elf_t* elf) {
    if (!elf) return;

    /* free program headers. */
    if (elf->phdrs) {
        _foreach(elf->phdrs, elf_phdr_t*, phdr)
            free(phdr);
        _endforeach;
        dyna_free(elf->phdrs);
    }

    /* free section headers. */
    if (elf->shdrs) {
        _foreach(elf->shdrs, elf_shdr_t*, shdr)
            free(shdr);
        _endforeach;
        dyna_free(elf->shdrs);
    }

    /* free string table. */
    free(elf->shstrtab);

    /* free path. */
    free(elf->path);
    free(elf);
}

/**
 * @brief get the name of a section header.
 *
 * @param elf the elf structure.
 * @param shdr the section header.
 * @return a pointer to the name if successful, 0x0 o.w.
 */
const char*
elf_shdr_name(elf_t* elf, elf_shdr_t* shdr) {
    if (!elf || !shdr || !elf->shstrtab) return 0x0;
    if (shdr->name >= elf->shstrtab_size) return 0x0;

    const char* s = elf->shstrtab + shdr->name;
    size_t remaining = elf->shstrtab_size - shdr->name;
    if (!memchr(s, 0, remaining)) return 0x0; /* no terminator in table. */
    return s;
}

/**
 * @brief get the capstone architecture tuple for an elf file.
 *
 * @param elf the elf structure.
 * @return the architecture tuple.
 */
tup_arch_t
elf_get_arch(elf_t* elf) {
    if (!elf) return (tup_arch_t){ .arch = CS_ARCH_X86, .mode = CS_MODE_64 };

    /* map elf machine to capstone architecture. */
    switch (elf->machine) {
        case ELF_MACH_X86:
            return (tup_arch_t){ .arch = CS_ARCH_X86, .mode = CS_MODE_32 };
        case ELF_MACH_X86_64:
            return (tup_arch_t){ .arch = CS_ARCH_X86, .mode = CS_MODE_64 };
        case ELF_MACH_ARM:
            return (tup_arch_t){ .arch = CS_ARCH_ARM, .mode = CS_MODE_ARM };
        case ELF_MACH_AARCH64:
            return (tup_arch_t){ .arch = CS_ARCH_AARCH64, .mode = CS_MODE_ARM };
        default:
            fprintf(stderr, "lzd, elf_get_arch; unsupported machine type %d, defaulting to x86-64.\n", elf->machine);
            return (tup_arch_t){ .arch = CS_ARCH_X86, .mode = CS_MODE_64 };
    }
}
