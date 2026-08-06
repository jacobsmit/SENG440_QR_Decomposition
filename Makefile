# ============================================================================
# SENG 440 QR Decomposition -- variant x flagset build matrix
#
#   make test-all        accuracy suite for every variant  (portable)
#   make profile-all     operation counts for every variant (portable)
#   make static-all      static instruction counts per flagset (needs ARM gcc)
#   make compare         build/summary.csv for the report tables
#   make asm VARIANT=x   -S listings for inspection
#   make clean
#
# Adding a variant: create src/variants/<name>/qr.c and add <name> to VARIANTS.
# ============================================================================

VARIANTS := naive_float fixed_scalar

# Flag sets for the static analysis live in profiling/flagsets.sh -- single
# source of truth, shared with arm_profile.sh. Do not duplicate them here.

# ALGO_SRC is the algorithm under measurement -- what the static ARM analysis
# counts. SUPPORT_SRC is test/report scaffolding, linked into the runnable
# binaries but deliberately excluded from the instruction counts.
ALGO_SRC    := src/common/matrix.c src/common/trig_pwl.c
SUPPORT_SRC := src/common/matrix_f32.c src/common/op_counters.c
COMMON_SRC  := $(ALGO_SRC) $(SUPPORT_SRC)
COMMON_HDR := $(wildcard src/common/*.h)
BUILD      := build

# --- toolchain -------------------------------------------------------------
# Native on the ARM VM; otherwise the host compiler for the portable targets
# and a cross-compiler (if present) for the ARM-specific static analysis.
UNAME_M := $(shell uname -m)
ifneq (,$(filter armv7l armv6l,$(UNAME_M)))
  CC        ?= gcc
  OBJDUMP   ?= objdump
  ARM_CC    := $(CC)
  ARM_DUMP  := $(OBJDUMP)
  PORTABLE_CFLAGS := -O2 -marm -mcpu=cortex-a7
else
  CC        ?= cc
  ARM_CC    := $(shell command -v arm-linux-gnueabihf-gcc 2>/dev/null)
  ARM_DUMP  := $(shell command -v arm-linux-gnueabihf-objdump 2>/dev/null)
  PORTABLE_CFLAGS := -O2
endif

WARN := -Wall -Wextra

.PHONY: all test-all profile-all static-all compare clean help asm
.PHONY: $(addprefix test-,$(VARIANTS)) $(addprefix profile-,$(VARIANTS))

all: test-all

help:
	@echo "targets: test-all profile-all static-all compare asm clean"
	@echo "variants: $(VARIANTS)"
	@echo "flagsets: (see profiling/flagsets.sh)"

ITERATIONS ?= 1000
MAGNITUDE  ?= 8

# ============================================================================
# Per-variant rules.
#
# Generated explicitly rather than with a pattern rule: GNU make excludes
# .PHONY targets from implicit/pattern rule matching, so "test-%: ..." silently
# does nothing once test-<variant> is declared PHONY (verified on make 3.81,
# which is what macOS ships). Explicit rules avoid the conflict entirely.
# ============================================================================
define VARIANT_RULES

$(BUILD)/$(1)/test: src/variants/$(1)/qr.c $(COMMON_SRC) $(COMMON_HDR) tests/test_qr_accuracy.c
	@mkdir -p $$(dir $$@)
	$(CC) $(WARN) $(PORTABLE_CFLAGS) -o $$@ \
	    tests/test_qr_accuracy.c src/variants/$(1)/qr.c $(COMMON_SRC) -lm

$(BUILD)/$(1)/profile: src/variants/$(1)/qr.c $(COMMON_SRC) $(COMMON_HDR) profiling/profile_ops.c
	@mkdir -p $$(dir $$@)
	$(CC) $(WARN) $(PORTABLE_CFLAGS) -DPROFILE_OPS -o $$@ \
	    profiling/profile_ops.c src/variants/$(1)/qr.c $(COMMON_SRC) -lm

test-$(1): $(BUILD)/$(1)/test
	@$$<

profile-$(1): $(BUILD)/$(1)/profile
	@$$< $$(ITERATIONS) $$(MAGNITUDE)

endef

$(foreach v,$(VARIANTS),$(eval $(call VARIANT_RULES,$(v))))

# Depend on the per-variant targets rather than looping in the shell: each
# test-<variant> exits non-zero on failure, so make aborts before the success
# banner can print. Correct by construction -- no shell flag to get wrong.
# (An earlier shell-loop version printed "ALL VARIANTS PASS" while sub-makes
# were failing, which is precisely the bug this suite exists to prevent.)
test-all: $(addprefix test-,$(VARIANTS))
	@echo "=========================================="
	@echo " ALL VARIANTS PASS"
	@echo "=========================================="

profile-all: $(addprefix profile-,$(VARIANTS))

# ============================================================================
# Static instruction counts -- needs an ARM toolchain
# ============================================================================
static-all:
	@if [ -z "$(ARM_CC)" ]; then \
	  echo "ERROR: no ARM toolchain. Run on the VM, or install arm-linux-gnueabihf-gcc."; \
	  exit 1; \
	fi
	@ARM_CC="$(ARM_CC)" ARM_DUMP="$(ARM_DUMP)" VARIANTS="$(VARIANTS)" \
	 ./profiling/arm_profile.sh

asm:
	@if [ -z "$(VARIANT)" ]; then echo "usage: make asm VARIANT=<name>"; exit 2; fi
	@if [ -z "$(ARM_CC)" ]; then echo "ERROR: no ARM toolchain."; exit 1; fi
	@mkdir -p $(BUILD)/$(VARIANT)/asm
	@. profiling/flagsets.sh; \
	 for fs in $$FLAGSETS; do \
	   $(ARM_CC) $$(flags_for $$fs) -Isrc/common -S \
	     -o $(BUILD)/$(VARIANT)/asm/qr.$$fs.s src/variants/$(VARIANT)/qr.c \
	   && echo "  wrote $(BUILD)/$(VARIANT)/asm/qr.$$fs.s"; \
	 done

# ============================================================================
# CSV summary for the report
# ============================================================================
# `compare` runs ALL THREE measurements and writes the report tables:
#   build/ops.csv     one row per variant: operation counts + accuracy
#   build/static.csv  one row per variant x flagset: instruction counts
#
# Two files rather than one because they are genuinely different tables --
# dynamic counts are per variant, static counts are per variant AND flagset.
# Denormalising them into a single CSV would duplicate every dynamic row.
#
# Accuracy is included deliberately: a fast wrong answer is not a data point,
# so `compare` exits non-zero if any variant fails its invariants.
compare:
	@mkdir -p $(BUILD)
	@echo ">>> operation counts + accuracy"
	@status=0; first=1; \
	for v in $(VARIANTS); do \
	  $(MAKE) -s $(BUILD)/$$v/profile $(BUILD)/$$v/test; \
	  prof=$$($(BUILD)/$$v/profile $(ITERATIONS) $(MAGNITUDE) --csv); \
	  acc=$$($(BUILD)/$$v/test --csv) || status=1; \
	  if [ $$first -eq 1 ]; then \
	    printf '%s,%s\n' "$$(echo "$$prof" | head -1)" \
	      "$$(echo "$$acc" | head -1 | cut -d, -f2-)" > $(BUILD)/ops.csv; \
	    first=0; \
	  fi; \
	  printf '%s,%s\n' "$$(echo "$$prof" | tail -1)" \
	    "$$(echo "$$acc" | tail -1 | cut -d, -f2-)" >> $(BUILD)/ops.csv; \
	done; \
	echo "wrote $(BUILD)/ops.csv"; \
	cat $(BUILD)/ops.csv; \
	echo; \
	if [ -n "$(ARM_CC)" ]; then \
	  echo ">>> static instruction counts"; \
	  $(MAKE) -s static-all > $(BUILD)/static.log 2>&1 || true; \
	  if [ -f $(BUILD)/static.csv ]; then cat $(BUILD)/static.csv; \
	  else echo "  (static analysis produced no CSV -- see $(BUILD)/static.log)"; fi; \
	else \
	  echo ">>> static instruction counts SKIPPED (no ARM toolchain on this host)"; \
	  echo "    run 'make compare' on the ARM VM to fill in build/static.csv"; \
	fi; \
	echo; \
	if [ $$status -ne 0 ]; then \
	  echo "!! at least one variant FAILED its accuracy invariants --"; \
	  echo "   its performance numbers above are not trustworthy."; \
	  exit 1; \
	fi

clean:
	rm -rf $(BUILD) profiling/_profile_out
