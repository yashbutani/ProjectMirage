# ============================================================
# Project Mirage — Build, Sync, Deploy, Run
# ============================================================
#
# Usage:
#   make              — Build everything + deploy to /tmp/mirage_demo
#   make build        — Compile the C validator only
#   make deploy       — Sync all files to /tmp/mirage_demo
#   make run          — Deploy + run the demo
#   make record       — Deploy + run the demo under asciinema
#   make clean        — Remove compiled binaries
#   make nuke         — Remove binaries + wipe /tmp/mirage_demo
#
# The root-level source files are the single source of truth.
# `make deploy` copies everything into /tmp/mirage_demo/ for runtime.
# ============================================================

CC       := gcc
CFLAGS   := -Wall -Wextra -O2
BINARY   := mirage_validation
SRC      := mirage_validation.c
DEPLOY   := /tmp/mirage_demo

# Honeypot data files
DATA_DIR := data
DATA_FILES := $(DATA_DIR)/shadow $(DATA_DIR)/id_rsa

# All source files that get deployed
SOURCES := mirage_agent.py poisoned_log.txt setup.sh Dockerfile

.PHONY: all build deploy run record clean nuke check-env help

# ── Default target ──────────────────────────────────────────
all: build deploy
	@echo ""
	@echo "✅  Build + deploy complete."
	@echo "    Run the demo with:  make run"
	@echo ""

# ── Compile ─────────────────────────────────────────────────
build: $(BINARY)

$(BINARY): $(SRC)
	@echo "🔨  Compiling $(SRC) → $(BINARY)"
	$(CC) $(CFLAGS) -o $(BINARY) $(SRC)
	@echo "    Done."

# ── Deploy to /tmp/mirage_demo ──────────────────────────────
deploy: build
	@echo ""
	@echo "📦  Deploying to $(DEPLOY)/"
	@mkdir -p $(DEPLOY)/data
	@cp -v $(BINARY)    $(DEPLOY)/$(BINARY)
	@cp -v $(SRC)       $(DEPLOY)/$(SRC)
	@for f in $(SOURCES); do \
		cp -v $$f $(DEPLOY)/$$f; \
	done
	@for f in $(DATA_FILES); do \
		cp -v $$f $(DEPLOY)/$$f; \
	done
	@# Also sync the mirage_demo/ subdirectory to match
	@mkdir -p mirage_demo/data
	@cp -v $(BINARY) mirage_demo/$(BINARY)
	@cp -v $(SRC)    mirage_demo/$(SRC)
	@for f in $(SOURCES); do \
		cp -v $$f mirage_demo/$$f; \
	done
	@for f in $(DATA_FILES); do \
		cp -v $$f mirage_demo/$$f; \
	done
	@echo ""
	@echo "✅  Deploy complete. $(DEPLOY)/ is in sync."

# ── Run the demo ────────────────────────────────────────────
check-env:
ifndef GEMINI_API_KEY
	$(error GEMINI_API_KEY is not set. Export it first: export GEMINI_API_KEY=your_key)
endif

run: deploy check-env
	@echo ""
	@echo "🚀  Running Project Mirage demo..."
	@echo "──────────────────────────────────────────"
	$(DEPLOY)/$(BINARY)

# ── Record with asciinema ───────────────────────────────────
record: deploy check-env
	@echo ""
	@echo "🎬  Recording demo with asciinema..."
	asciinema rec --command "$(DEPLOY)/$(BINARY)" mirage_demo.cast --overwrite
	@echo "✅  Recording saved to mirage_demo.cast"

# ── Clean ───────────────────────────────────────────────────
clean:
	@echo "🧹  Cleaning build artifacts..."
	rm -f $(BINARY)
	rm -f mirage_demo/$(BINARY)
	@echo "    Done."

nuke: clean
	@echo "💣  Wiping $(DEPLOY)/..."
	rm -rf $(DEPLOY)
	@echo "    Done."

# ── Help ────────────────────────────────────────────────────
help:
	@echo ""
	@echo "Project Mirage — Build Targets"
	@echo "────────────────────────────────────────────────"
	@echo "  make          Build + deploy (default)"
	@echo "  make build    Compile the C validator"
	@echo "  make deploy   Sync all files to $(DEPLOY)/"
	@echo "  make run      Deploy + run the demo"
	@echo "  make record   Deploy + record demo via asciinema"
	@echo "  make clean    Remove compiled binaries"
	@echo "  make nuke     Remove binaries + wipe $(DEPLOY)/"
	@echo "  make help     Show this message"
	@echo ""
