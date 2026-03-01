cc ?= gcc
rm := rm -rf
mkdir := mkdir -p

# architecture selection.
arch ?= x86_64

# directories.
src_dir := src
build_dir := build/$(arch)
vendor_dir := vendor/build/$(arch)

# flags.
cflags ?= -std=c17 -Wall -Wextra -g -O0 -D_GNU_SOURCE

release ?= 0
ifeq ($(release),1)
  cflags := -std=c17 -Wall -Wextra -O2 -D_GNU_SOURCE
endif

# includes / libs.
cflags  += -I$(vendor_dir)/include
ldflags :=
ldlibs  := $(vendor_dir)/lib/libcapstone.a -lncursesw

# source discovery.
src := $(shell find $(src_dir) -type f -name '*.c')
obj := $(patsubst $(src_dir)/%.c,$(build_dir)/%.o,$(src))
target := lzd

# build rules.
all: $(build_dir) $(target)

$(build_dir):
	$(mkdir) $(build_dir)

$(target): $(obj)
	$(cc) $(obj) $(ldflags) $(ldlibs) -o $@

$(build_dir)/%.o: $(src_dir)/%.c
	$(mkdir) $(dir $@)
	$(cc) $(cflags) -c $< -o $@

# clean
clean:
	$(rm) build
	$(rm) $(target)

.PHONY: all clean