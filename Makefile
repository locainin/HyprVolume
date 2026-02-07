CC ?= gcc
PKG_CONFIG ?= pkg-config

# Build artifact and source layout.
TARGET := hyprvolume
SRC_DIR := src
PKGS := gtk4 gtk4-layer-shell-0

# Optional build controls:
# WARN_AS_ERR=1 promotes all warnings to hard errors.
WARN_AS_ERR ?= 0
BIN_DIR ?= $(HOME)/.local/bin
CONFIG_DIR ?= $(HOME)/.config/hyprvolume

SRCS := $(shell find $(SRC_DIR) -type f -name '*.c' | sort)
PKG_CFLAGS := $(shell $(PKG_CONFIG) --cflags $(PKGS))
PKG_LIBS := $(shell $(PKG_CONFIG) --libs $(PKGS))

CPPFLAGS += -I$(SRC_DIR) $(PKG_CFLAGS)
CFLAGS += -std=c11 -O2 -g -fno-omit-frame-pointer \
	-Wall -Wextra -Wformat=2 -Wconversion -Wshadow -Wpedantic
LDLIBS += $(PKG_LIBS)
LDLIBS += -lm

ifeq ($(WARN_AS_ERR),1)
CFLAGS += -Werror
endif

.PHONY: all clean check strict compdb install install-reset-config install-reset-style uninstall uninstall-purge

all: $(TARGET)

# Single-link build keeps local workspace free of .o caches.
$(TARGET): $(SRCS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SRCS) $(LDFLAGS) $(LDLIBS) -o $@

# Smoke test ensures the built binary starts and parses CLI.
check: $(TARGET)
	./$(TARGET) --help > /dev/null

strict:
	$(MAKE) clean
	$(MAKE) WARN_AS_ERR=1 check

compdb:
	./scripts/gen_compile_commands.sh

# Install targets keep default config/style behavior explicit.
install: $(TARGET)
	./scripts/install.sh --bin-source ./$(TARGET) --bin-dir "$(BIN_DIR)" --config-dir "$(CONFIG_DIR)"

install-reset-config: $(TARGET)
	./scripts/install.sh --bin-source ./$(TARGET) --bin-dir "$(BIN_DIR)" --config-dir "$(CONFIG_DIR)" --overwrite-config

install-reset-style: $(TARGET)
	./scripts/install.sh --bin-source ./$(TARGET) --bin-dir "$(BIN_DIR)" --config-dir "$(CONFIG_DIR)" --overwrite-style

uninstall:
	./scripts/uninstall.sh --bin-dir "$(BIN_DIR)" --config-dir "$(CONFIG_DIR)"

uninstall-purge:
	./scripts/uninstall.sh --bin-dir "$(BIN_DIR)" --config-dir "$(CONFIG_DIR)" --purge

# Clean remains conservative and removes known local build artifacts only.
clean:
	rm -rf build build-san $(TARGET)
