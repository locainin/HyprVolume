CC ?= gcc
PKG_CONFIG ?= pkg-config

# Build artifact and source layout.
TARGET ?= hyprvolume
SRC_DIR ?= src
PKGS := gtk4 gtk4-layer-shell-0

# Build/install paths.
BIN_DIR ?= $(HOME)/.local/bin
CONFIG_DIR ?= $(HOME)/.config/hyprvolume

# Build controls:
# WARN_AS_ERR=1 promotes warnings to hard errors for strict/test profiles.
WARN_AS_ERR ?= 0

SRCS := $(shell find $(SRC_DIR) -type f -name '*.c' | sort)
PKG_CFLAGS_RAW := $(shell $(PKG_CONFIG) --cflags $(PKGS))
# External dependency headers are treated as system includes so strict clang
# profiles do not fail on third-party header diagnostics
PKG_CFLAGS := $(patsubst -I%,-isystem %,$(PKG_CFLAGS_RAW))
PKG_LIBS := $(shell $(PKG_CONFIG) --libs $(PKGS))

# Compiler flag profiles:
# - default `make`: optimized release-oriented build
# - `make strict`: warning-heavy diagnostics and warnings-as-errors
# - `make test`: sanitizer-focused runtime checks
CFLAGS_BASE ?= -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra
CFLAGS_RELEASE ?= -O3 -march=native -flto -DNDEBUG
CFLAGS_STRICT ?= -O2 -g -fno-omit-frame-pointer -Wformat=2 -Wconversion -Wshadow -Wpedantic
CFLAGS_TEST ?= -O2 -g -fno-omit-frame-pointer -fsanitize=address,undefined,leak -D_FORTIFY_SOURCE=3 -fstack-protector-strong -fno-sanitize-recover=all -Wformat=2 -Wconversion -Wshadow -Wpedantic

ACTIVE_CFLAGS ?= $(CFLAGS_RELEASE)
LDFLAGS_EXTRA ?=
WARN_AS_ERR_FLAG :=

CPPFLAGS += -I$(SRC_DIR) $(PKG_CFLAGS)
LDFLAGS += $(LDFLAGS_EXTRA)
LDLIBS += $(PKG_LIBS) -lm

ifeq ($(WARN_AS_ERR),1)
WARN_AS_ERR_FLAG := -Werror
endif

.PHONY: all clean check strict test compdb install install-reset-config install-reset-style uninstall uninstall-purge

all: $(TARGET)

# Single-link build keeps local workspace free of .o caches.
$(TARGET): $(SRCS)
	@echo "[build] Target: $(TARGET)"
	@echo "[build] Compiler: $(CC)"
	@echo "[build] Active CFLAGS: $(CFLAGS_BASE) $(ACTIVE_CFLAGS) $(WARN_AS_ERR_FLAG)"
	$(CC) $(CPPFLAGS) $(CFLAGS_BASE) $(ACTIVE_CFLAGS) $(WARN_AS_ERR_FLAG) $(SRCS) $(LDFLAGS) $(LDLIBS) -o $@
	@echo "[build] Completed: ./$(TARGET)"

# Smoke test ensures the built binary starts and parses CLI.
check: $(TARGET)
	@echo "[check] Running CLI smoke test"
	./$(TARGET) --help > /dev/null
	@echo "[check] Passed"

strict:
	@echo "[strict] Rebuilding with strict diagnostics"
	@$(MAKE) clean
	@$(MAKE) ACTIVE_CFLAGS="$(CFLAGS_STRICT)" WARN_AS_ERR=1 check
	@echo "[strict] Passed"

test:
	@echo "[test] Rebuilding with sanitizer-focused test profile"
	@$(MAKE) clean
	@$(MAKE) ACTIVE_CFLAGS="$(CFLAGS_TEST)" LDFLAGS_EXTRA="-fsanitize=address,undefined,leak" WARN_AS_ERR=1 check
	@echo "[test] Passed"

compdb:
	@echo "[compdb] Generating compile_commands.json"
	./scripts/gen_compile_commands.sh

# Install targets keep default config/style behavior explicit.
install: $(TARGET)
	@echo "[install] Installing binary/config/snippet"
	./scripts/install.sh --bin-source ./$(TARGET) --bin-dir "$(BIN_DIR)" --config-dir "$(CONFIG_DIR)"

install-reset-config: $(TARGET)
	@echo "[install] Resetting shipped config and style"
	./scripts/install.sh --bin-source ./$(TARGET) --bin-dir "$(BIN_DIR)" --config-dir "$(CONFIG_DIR)" --overwrite-config

install-reset-style: $(TARGET)
	@echo "[install] Resetting shipped style only"
	./scripts/install.sh --bin-source ./$(TARGET) --bin-dir "$(BIN_DIR)" --config-dir "$(CONFIG_DIR)" --overwrite-style

uninstall:
	@echo "[uninstall] Removing binary and startup hook (keeping config)"
	./scripts/uninstall.sh --bin-dir "$(BIN_DIR)" --config-dir "$(CONFIG_DIR)"

uninstall-purge:
	@echo "[uninstall] Removing binary, startup hook, and config"
	./scripts/uninstall.sh --bin-dir "$(BIN_DIR)" --config-dir "$(CONFIG_DIR)" --purge

# Clean remains conservative and removes known local build artifacts only.
clean:
	@echo "[clean] Removing local build artifacts"
	rm -rf build build-san $(TARGET)
