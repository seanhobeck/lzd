cc ?= gcc
rm := rm -rf
mkdir := mkdir -p

# architecture selection.
arch ?= x86_64

# directories.
src_dir := src
build_dir := build/$(arch)
vendor_dir := vendor/install/capstone/$(arch)

# flags.
cflags ?= -std=c17 -Wall -Wextra -g -O0

release ?= 0
ifeq ($(release),1)
  cflags := -std=c17 -Wall -Wextra -O2
endif

cflags += -I$(vendor_dir)/include
ldflags := -L$(vendor_dir)/lib
ldlibs := -lcapstone -lncursesw

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
	$(cc) $(cflags) -c $< -o $@

# clean
clean:
	$(rm) build
	$(rm) $(target)

.PHONY: all clean