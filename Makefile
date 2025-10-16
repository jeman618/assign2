# Cross-platform Makefile for liblwp + demos
# - macOS: builds liblwp.dylib and numbers
# - Linux: builds liblwp.so, numbers, randomsnakes, hungrysnakes

CC       := clang
UNAME_S  := $(shell uname -s)

# Course include dir (per Piazza)
COURSE_INC := -I~pn-cs453/Given/Asgn2/include
LOCAL_INC  := -I.

# Base flags (use course include FIRST, then local)
BASE_CFLAGS := -Wall -Wextra -Werror -O2 -fPIC -std=c99 $(COURSE_INC) $(LOCAL_INC)

ifeq ($(UNAME_S),Darwin)
  ARCH      := -arch x86_64
  CFLAGS    := $(BASE_CFLAGS) $(ARCH)
  SO        := liblwp.dylib
  SOLINK    := -dynamiclib -install_name @rpath/$(SO) $(ARCH)
  DEMOS     := numbers
else
  CC        := gcc
  CFLAGS    := -Wall -Wextra -Werror -O2 -fPIC -std=c99 $(COURSE_INC) $(LOCAL_INC)
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
# NOTE: remove 'fp.h' from prerequisites so Make doesn't require it locally.
$(SO): $(OBJ) lwp.h
	$(CC) $(SOLINK) -o $@ $(OBJ)

# Object rules
%.o: %.c
	$(CC) $(CFLAGS) -c $<

%.o: %.S
	$(CC) $(ARCH) -c $<

# ---- Demos ----
numbers: numbersmain.c lwp.h
	$(CC) $(filter-out -Werror,$(CFLAGS)) -Wno-cast-function-type-mismatch -Wno-unused-parameter -o $@ numbersmain.c -L. -llwp

randomsnakes: randomsnakes.c snakes.h util.h lwp.h
	$(CC) $(CFLAGS) -o $@ randomsnakes.c util.c -L. -llwp $(SNAKESLIB) $(CURSES)

hungrysnakes: hungrysnakes.c snakes.h util.h lwp.h
	$(CC) $(CFLAGS) -o $@ hungrysnakes.c util.c -L. -llwp $(SNAKESLIB) $(CURSES)

clean:
	rm -f $(OBJ) $(SO) numbers randomsnakes hungrysnakes
