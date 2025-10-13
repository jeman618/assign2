# Cross-platform Makefile for liblwp + demos
# - macOS (Darwin/Rosetta x86_64): builds liblwp.dylib and numbers
# - Linux: builds liblwp.so, numbers, randomsnakes, hungrysnakes

CC       := clang
UNAME_S  := $(shell uname -s)

# Base flags for the library build (strict)
BASE_CFLAGS := -Wall -Wextra -Werror -O2 -fPIC -std=c99

# macOS (Darwin) specifics: force x86_64 so __x86_64__ is defined
ifeq ($(UNAME_S),Darwin)
  ARCH      := -arch x86_64
  CFLAGS    := $(BASE_CFLAGS) $(ARCH)
  SO        := liblwp.dylib
  SOLINK    := -dynamiclib -install_name @rpath/$(SO) $(ARCH)
  # Only build numbers on macOS (the snakes .so files are Linux)
  DEMOS     := numbers
else
  # Linux
  CC        := gcc
  CFLAGS    := -Wall -Wextra -Werror -O2 -fPIC -std=c99
  SO        := liblwp.so
  SOLINK    := -shared -Wl,-soname,$(SO)
  DEMOS     := numbers randomsnakes hungrysnakes
  CURSES    := -lncurses
  SNAKESLIB := -L. -lsnakes -lPLN
endif

SRC      := lwp.c magic64.S
OBJ      := $(SRC:.c=.o)
OBJ      := $(OBJ:.S=.o)

.PHONY: all clean

all: $(SO) $(DEMOS)

# Shared library
$(SO): $(OBJ) lwp.h fp.h
	$(CC) $(SOLINK) -o $@ $(OBJ)

# Object rules
%.o: %.c
	$(CC) $(CFLAGS) -c $<

%.o: %.S
	$(CC) $(ARCH) -c $<

# ---- Demos ----
# Build 'numbers' but relax warnings ONLY for the professor's file.
numbers: numbersmain.c lwp.h
	$(CC) $(filter-out -Werror,$(CFLAGS)) -Wno-cast-function-type-mismatch -Wno-unused-parameter -o $@ numbersmain.c -L. -llwp

# Linux-only snakes demos (guarded by platform selection above)
randomsnakes: randomsnakes.c snakes.h util.h lwp.h
	$(CC) $(CFLAGS) -o $@ randomsnakes.c util.c -L. -llwp $(SNAKESLIB) $(CURSES)

hungrysnakes: hungrysnakes.c snakes.h util.h lwp.h
	$(CC) $(CFLAGS) -o $@ hungrysnakes.c util.c -L. -llwp $(SNAKESLIB) $(CURSES)

clean:
	rm -f $(OBJ) $(SO) numbers randomsnakes hungrysnakes
