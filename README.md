# lzd — Lazy Disassembler

A terminal-based ELF disassembler written in C17 using ncurses and capstone.
`lzd` focuses on lazy loading, region-based disassembly, and a minimal but clean terminal UI. 
It is designed as a lightweight reverse-engineering and binary exploration tool.

---

## Features

- ELF32 and ELF64 parsing,
- Section and segment inspection,
- Lazy region-based disassembly,
- Capstone-powered instruction decoding,
- TUI powered by ncurses,
- A disassembly view (instructions),
- A strings view (ANSI or C-strings from `.strtab` and `.rodata`),
- Symbols view (`.symtab` and `.dynsym`),
- Command bar (`goto`, `open`, etc.),
- Scrollable interface with keyboard navigation.

---

## Build

### Dependencies

- GCC or Clang
- `ncursesw`
- `libcapstone` (not required as it is included as a submodule).

To get started with `lzd`, clone the repo and build it like so:

```bash
# Clone the repo
git clone https://github.com/sean-hobeck/lzd.git
cd lzd

# Gather and build dependencies
sudo ./get-deps

# Build lzd's release binary
make release=1
```

### Run

Running `lzd` is as simple as:

```bash
./lzd
```

A little lost? Here are the supported commands and their usage:

- `open <path>` — load a ELF binary
- `goto <addr>` — jump to instruction address (hex or decimal)
- `view: <instructions>|<strings>|<symbols>` - jump to a specific view for instructions, strings,
  or symbols

---

## Roadmap

Planned improvements:

- Semantic analysis,
- Cross-reference analysis,
- Proper caching layer for decoded instructions,
- Analysis of both RUNPE and DWARF executable formats,
- Support for even more little-endian architectures (e.g. POWERPC, and RISC-V).
