# Cross-platform Makefile for liblwp + demos
# - macOS: builds liblwp.dylib and numbers
# - Linux: builds liblwp.so, numbers, randomsnakes, hungrysnakes

UNAME_S := $(shell uname -s)

# Resolve the prof's home dir once so tilde expansion works under make/sh
COURSE_HOME := $(shell bash -lc 'echo ~pn-cs453')
COURSE_INC  := $(COURSE_HOME)/Given/Asgn2/include

# Common flags: search course includes FIRST, then local
COMMON_CFLAGS := -Wall -Wextra -Werror -O2 -fPIC -std=c99 \
                 -I$(COURSE_INC) -I.

ifeq ($(UNAME_S),Darwin)
  CC      := clang
  ARCH    := -arch x86_64
  CFLAGS  := $(COMMON_CFLAGS) $(ARCH)
  SO      := liblwp.dylib
  SOLINK  := -dynamiclib -install_name @rpath/$(SO) $(ARCH)
  DEMOS   := numbers
else
  CC      := gcc
  CFLAGS  := $(COMMON_CFLAGS)
  SO      := liblwp.so
  SOLINK  := -shared -Wl,-soname,$(SO)
  DEMOS   := numbers randomsnakes hungrysnakes
  CURSES  := -lncurses
  SNAKESLIB := -L. -lsnakes -lPLN
endif

SRC := lwp.c magic64.S
OBJ := $(SRC:.c=.o)
OBJ := $(OBJ:.S=.o)

.PHONY: all clean
all: $(SO) $(DEMOS)

# Shared library (do NOT require fp.h locally)
$(SO): $(OBJ) lwp.h
	$(CC) $(SOLINK) -o $@ $(OBJ)

# Objects
%.o: %.c
	$(CC) $(CFLAGS) -c $<

%.o: %.S
	$(CC) $(ARCH) -c $<

# Demos
numbers: numbersmain.c lwp.h
	$(CC) $(filter-out -Werror,$(CFLAGS)) \
	      -Wno-cast-function-type-mismatch -Wno-unused-parameter \
	      -o $@ $< -L. -llwp

randomsnakes: randomsnakes.c snakes.h util.h lwp.h
	$(CC) $(CFLAGS) -o $@ randomsnakes.c util.c -L. -llwp $(SNAKESLIB) $(CURSES)

hungrysnakes: hungrysnakes.c snakes.h util.h lwp.h
	$(CC) $(CFLAGS) -o $@ hungrysnakes.c util.c -L. -llwp $(SNAKESLIB) $(CURSES)

clean:
	rm -f $(OBJ) $(SO) numbers randomsnakes hungrysnakes
