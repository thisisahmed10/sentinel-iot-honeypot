# Makefile for pi_bait (Raspberry Pi 4 component)
# Supports optional UNP library include/lib paths via UNP_DIR/UNP_LIBDIR

CC := gcc
CFLAGS := -std=c11 -Wall -Wextra -O2 -g

SRCDIR := src
INCLUDEDIR := include
HW_INCDIR := hardware/include
BINDIR := bin
TARGET := $(BINDIR)/pi_bait

# Optional: specify UNP headers and lib dirs when invoking make:
# make UNP_DIR=/path/to/unpv13e/include UNP_LIBDIR=/path/to/unpv13e/lib
UNP_DIR ?= /usr/local/include
UNP_LIBDIR ?= /usr/local/lib
# Leave UNP_LIBS empty by default; set to -lunp when linking against installed unp library
UNP_LIBS ?=

INCLUDES := -I$(INCLUDEDIR) -I$(HW_INCDIR) -I$(UNP_DIR)
LDFLAGS :=
ifneq ($(strip $(UNP_LIBS)),)
LDFLAGS := -L$(UNP_LIBDIR) $(UNP_LIBS)
endif

SRCS := $(wildcard $(SRCDIR)/*.c)

all: $(TARGET)

$(TARGET): $(SRCS)
	mkdir -p $(BINDIR)
	$(CC) $(CFLAGS) $(INCLUDES) $^ -o $@ $(LDFLAGS)

clean:
	rm -rf $(BINDIR)/*.o $(TARGET)

install: $(TARGET)
	@echo "Built $(TARGET)"

.PHONY: all clean install
