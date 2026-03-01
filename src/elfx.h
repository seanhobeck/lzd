/**
 * @author Sean Hobeck
 * @date 2026-02-28
 */
#ifndef LZD_ELFX_H
#define LZD_ELFX_H

/*! @uses uint8_t, uint16_t, uint32_t, uint64_t. */
#include <stdint.h>

/*! @uses ssize_t, pid_t. */
#include <sys/types.h>

/*! @uses bool. */
#include <stdbool.h>

/*! @uses dyna_t. */
#include "dyna.h"

/* elf class. */
typedef enum {
    ELF_CLASS_NONE = 0u,
    ELF_CLASS_32 = 1u,
    ELF_CLASS_64 = 2u
} elf_class_t;

/* elf data encoding. */
typedef enum {
    ELF_DATA_NONE = 0u,
    ELF_DATA_LSB = 1u, /* little endian. */
    ELF_DATA_MSB = 2u  /* big endian. */
} elf_data_t;

/* elf type. */
typedef enum {
    ELF_TYPE_NONE = 0u,
    ELF_TYPE_REL = 1u,  /* relocatable. */
    ELF_TYPE_EXEC = 2u, /* executable. */
    ELF_TYPE_DYN = 3u,  /* shared object. */
    ELF_TYPE_CORE = 4u  /* core file. */
} elf_type_t;

/* elf machine. */
typedef enum {
    ELF_MACH_NONE = 0u,
    ELF_MACH_X86 = 3u,
    ELF_MACH_ARM = 40u,
    ELF_MACH_X86_64 = 62u,
    ELF_MACH_AARCH64 = 183u
} elf_mach_t;

/* elf program header type. */
typedef enum {
    ELF_PT_NULL = 0u,
    ELF_PT_LOAD = 1u,
    ELF_PT_DYNAMIC = 2u,
    ELF_PT_INTERP = 3u,
    ELF_PT_NOTE = 4u,
    ELF_PT_SHLIB = 5u,
    ELF_PT_PHDR = 6u,
    ELF_PT_TLS = 7u
} elf_phdr_type_t;

/* elf section header type. */
typedef enum {
    ELF_SHT_NULL = 0u,
    ELF_SHT_PROGBITS = 1u,
    ELF_SHT_SYMTAB = 2u,
    ELF_SHT_STRTAB = 3u,
    ELF_SHT_RELA = 4u,
    ELF_SHT_HASH = 5u,
    ELF_SHT_DYNAMIC = 6u,
    ELF_SHT_NOTE = 7u,
    ELF_SHT_NOBITS = 8u,
    ELF_SHT_REL = 9u,
    ELF_SHT_SHLIB = 10u,
    ELF_SHT_DYNSYM = 11u
} elf_shdr_type_t;

/* program header flags. */
#define ELF_PF_X 0x1 /* executable. */
#define ELF_PF_W 0x2 /* writable. */
#define ELF_PF_R 0x4 /* readable. */

/* section header flags. */
#define ELF_SHF_WRITE 0x1 /* writable. */
#define ELF_SHF_ALLOC 0x2 /* occupies memory. */
#define ELF_SHF_EXECINSTR 0x4 /* executable. */

/* ... */
typedef struct {
    uint32_t type; /* segment type. */
    uint32_t flags; /* segment flags. */
    uint64_t offset; /* file offset. */
    uint64_t vaddr; /* virtual address. */
    uint64_t paddr; /* physical address. */
    uint64_t filesz; /* size in file. */
    uint64_t memsz; /* size in memory. */
    uint64_t align; /* alignment. */
} elf_phdr_t;

/* ... */
typedef struct {
    uint32_t name; /* section name (string tbl index). */
    uint32_t type; /* section type. */
    uint64_t flags; /* section flags. */
    uint64_t addr; /* virtual address. */
    uint64_t offset; /* file offset. */
    uint64_t size; /* section size. */
    uint32_t link; /* link to another section. */
    uint32_t info; /* additional info. */
    uint64_t addralign; /* alignment. */
    uint64_t entsize; /* entry size if section holds table. */
} elf_shdr_t;

/* ... */
typedef struct {
    elf_class_t class; /* 32 or 64 bit. */
    elf_data_t data; /* endianness. */
    elf_type_t type; /* executable, relocatable, etc... */
    elf_mach_t machine; /* target architecture. */
    uint64_t entry; /* entry point address. */
    uint64_t phoff; /* program header table offset. */
    uint64_t shoff; /* section header table offset. */
    uint16_t phnum; /* number of program headers. */
    uint16_t shnum; /* number of section headers. */
    uint16_t shstrndx; /* section header string table index. */
    dyna_t* phdrs; /* dynamic array of elf_phdr_t*. */
    dyna_t* shdrs; /* dynamic array of elf_shdr_t*. */
    char* shstrtab; /* section header string table. */
    size_t shstrtab_size; /* size of shstrtab. */
} elf_t;

/**
 * @brief parse an elf file from a file path.
 *
 * @param path the path to the elf file.
 * @return a pointer to an allocated elf_t if successful, 0x0 o.w.
 */
elf_t*
elf_parse(const char* path);

/**
 * @brief free an elf structure and all of its data.
 *
 * @param elf the elf structure to be freed.
 */
void
elf_free(elf_t* elf);

/**
 * @brief get the name of a section header.
 *
 * @param elf the elf structure.
 * @param shdr the section header.
 * @return a pointer to the name if successful, 0x0 o.w.
 */
const char*
elf_shdr_name(elf_t* elf, elf_shdr_t* shdr);

/**
 * @brief get the capstone architecture tuple for an elf file.
 *
 * @param elf the elf structure.
 * @return the architecture tuple.
 */
#include "arch.h"
tup_arch_t
elf_get_arch(elf_t* elf);
#endif /* LZD_ELFX_H */
