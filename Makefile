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

VARIANTS := naive_float fixed_scalar fixed_simd32 fixed_asip

# Variants exposing a native float entry point (measured without fixed-point
# conversion, since a naive implementation would never do that).
EXTRA_naive_float := -DVARIANT_HAS_F32

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

# Measurement-build only. The qr_profiled() wrappers exist so callgrind can
# toggle collection on a known, non-recursive symbol. At -O2 gcc turns them
# into TAIL CALLS ("b qr_decomposition" instead of push/bl/pop) -- callgrind
# then sees the function entered but never left, so --toggle-collect never
# switches collection back off and printf/malloc/libc pour into the totals.
# Disabling sibling-call optimisation keeps the return visible.
# Costs 3 instructions per measured call (~0.1%), inside the measured region.
PROFILE_CFLAGS := -fno-optimize-sibling-calls

.PHONY: all test-all profile-all static-all instr-all instr-detail cycles asip-asm compare clean help asm pwl-tables pwl-sweep
.PHONY: $(addprefix test-,$(VARIANTS)) $(addprefix profile-,$(VARIANTS))

all: test-all

help:
	@echo "targets: test-all profile-all static-all instr-all instr-detail cycles asip-asm compare asm clean"
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
	$(CC) $(WARN) $(PORTABLE_CFLAGS) $(PROFILE_CFLAGS) -DPROFILE_OPS $$(EXTRA_$(1)) -o $$@ \
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
# ============================================================================
# Exact dynamic instruction counts (callgrind). This is the metric that shows
# whether an optimisation worked for EVERY kind of change -- including inline
# assembly and loop unrolling, which alter the instruction stream without
# changing operation counts and are therefore invisible in the op-count table.
#
# Slow: callgrind instruments every instruction, on top of QEMU's own overhead.
# Hence the much smaller default iteration count -- the per-QR figure is
# deterministic, so a small sample is enough.
# ============================================================================
CG_ITERATIONS ?= 50

# Per-function breakdown: where do the instructions actually go? Answers
# "is the bottleneck the trig or the rotations", which decides which
# optimisation to do first.
# ============================================================================
# Cycle estimate: exact dynamic OPCODE histogram, weighted by Cortex-A7
# latencies. Answers "is this actually faster", which instruction counts
# cannot -- an SDIV is not a MOV.
#
#   make cycles VARIANT=fixed_scalar
#   make cycles VARIANT=naive_float FLOAT=--float
# ============================================================================
LIBM := $(wildcard /usr/lib/arm-linux-gnueabihf/libm.so.6)
LIBC := $(wildcard /usr/lib/arm-linux-gnueabihf/libc.so.6)

cycles:
	@if [ -z "$(VARIANT)" ]; then \
	  echo "usage: make cycles VARIANT=<name> [FLOAT=--float]"; exit 2; fi
	@if ! command -v valgrind >/dev/null 2>&1; then \
	  echo "ERROR: valgrind not installed"; exit 1; fi
	@$(MAKE) -s $(BUILD)/$(VARIANT)/profile
	@mkdir -p $(BUILD)/callgrind
	@valgrind --tool=callgrind \
	   --callgrind-out-file=$(BUILD)/callgrind/$(VARIANT).instr \
	   --dump-instr=yes --collect-atstart=no \
	   --toggle-collect=$(if $(FLOAT),qr_profiled_f32,qr_profiled) \
	   --quiet $(BUILD)/$(VARIANT)/profile $(CG_ITERATIONS) 8 $(FLOAT) \
	   >/dev/null 2>$(BUILD)/callgrind/$(VARIANT).instr.log
	@echo "=== $(VARIANT)$(if $(FLOAT), (float interface),): opcode histogram over $(CG_ITERATIONS) QRs ==="
	@python3 profiling/cycles.py $(BUILD)/callgrind/$(VARIANT).instr \
	   $(BUILD)/$(VARIANT)/profile $(LIBM) $(LIBC)
	@echo
	@echo "Divide by $(CG_ITERATIONS) for per-QR figures."

# ============================================================================
# The custom instruction, instantiated for real.
#
# Produces an assembly listing containing "GIVENSQ Rd, Rn, Rm" and stops. The
# assembler has no opcode for it, so this can never be linked -- Lesson 100:
# "You cannot assemble such code, since the assembler cannot allocate an
# operation code for EXECUTE_B". The failure is the expected outcome and is
# demonstrated, not hidden.
#
# The runnable, accuracy-tested build of the same variant uses the C reference
# model instead (plain `make test-fixed_asip`).
# ============================================================================
asip-asm:
	@if [ -z "$(ARM_CC)" ]; then echo "ERROR: no ARM toolchain."; exit 1; fi
	@mkdir -p $(BUILD)/fixed_asip
	@. profiling/flagsets.sh; \
	 flags=$$(flags_for cortex-a7); \
	 echo ">>> compiling with -DUSE_GIVENSQ_ASM $$flags -S"; \
	 $(ARM_CC) $$flags -Isrc/common -DUSE_GIVENSQ_ASM -S \
	   -o $(BUILD)/fixed_asip/qr.givensq.s src/variants/fixed_asip/qr.c \
	 && echo "    wrote $(BUILD)/fixed_asip/qr.givensq.s"
	@echo
	@echo ">>> the instruction in the generated listing:"
	@grep -n -B2 -A2 "GIVENSQ" $(BUILD)/fixed_asip/qr.givensq.s | sed 's/^/    /' \
	  || { echo "    !! GIVENSQ not found -- inline asm did not survive"; exit 1; }
	@echo
	@echo ">>> now try to assemble it (this MUST fail -- that is the point):"
	@. profiling/flagsets.sh; \
	 flags=$$(flags_for cortex-a7); \
	 if $(ARM_CC) $$flags -c -o $(BUILD)/fixed_asip/qr.givensq.o \
	      $(BUILD)/fixed_asip/qr.givensq.s 2>$(BUILD)/fixed_asip/as.log; then \
	   echo "    !! UNEXPECTED: it assembled. The mnemonic collided with a real"; \
	   echo "       instruction -- pick a different name."; exit 1; \
	 else \
	   echo "    assembler rejected it, as expected:"; \
	   head -3 $(BUILD)/fixed_asip/as.log | sed 's/^/      /'; \
	 fi

instr-detail:
	@if [ -z "$(VARIANT)" ]; then echo "usage: make instr-detail VARIANT=<name>"; exit 2; fi
	@if ! command -v valgrind >/dev/null 2>&1; then \
	  echo "ERROR: valgrind not installed"; exit 1; fi
	@$(MAKE) -s $(BUILD)/$(VARIANT)/profile
	@mkdir -p $(BUILD)/callgrind
	@valgrind --tool=callgrind \
	   --callgrind-out-file=$(BUILD)/callgrind/$(VARIANT).detail \
	   --collect-atstart=no --toggle-collect=qr_decomposition --quiet \
	   $(BUILD)/$(VARIANT)/profile $(CG_ITERATIONS) 8 >/dev/null 2>&1
	@echo "=== $(VARIANT): instructions per function ($(CG_ITERATIONS) QRs) ==="
	@callgrind_annotate --threshold=99 \
	   $(BUILD)/callgrind/$(VARIANT).detail 2>/dev/null \
	 | awk '/PROGRAM TOTALS/{t=1} t' \
	 | head -30
	@echo
	@echo "Divide the per-function counts by $(CG_ITERATIONS) for per-QR figures."

instr-all:
	@if ! command -v valgrind >/dev/null 2>&1; then \
	  echo "ERROR: valgrind not installed (apt-get install valgrind)"; exit 1; fi
	@mkdir -p $(BUILD)
	@echo "variant,cg_iterations,ir_total,ir_per_qr" > $(BUILD)/instr.csv
	@for v in $(VARIANTS); do \
	  $(MAKE) -s $(BUILD)/$$v/profile; \
	  echo "  running callgrind on $$v ($(CG_ITERATIONS) iterations, this is slow)..."; \
	  ./profiling/callgrind_count.sh $$v $(CG_ITERATIONS) >> $(BUILD)/instr.csv \
	    || { echo "  !! callgrind failed for $$v"; exit 1; }; \
	done
	@echo "wrote $(BUILD)/instr.csv"
	@cat $(BUILD)/instr.csv

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
	if command -v valgrind >/dev/null 2>&1; then \
	  echo; echo ">>> exact dynamic instruction counts (callgrind, $(CG_ITERATIONS) iters)"; \
	  if $(MAKE) -s instr-all > $(BUILD)/instr.log 2>&1; then \
	    awk -F, 'NR==FNR{if(FNR>1)ir[$$1]=$$4; next} \
	             FNR==1{print $$0",ir_per_qr"; next} \
	             {print $$0","(($$1 in ir)?ir[$$1]:"n/a")}' \
	        $(BUILD)/instr.csv $(BUILD)/ops.csv > $(BUILD)/ops_full.csv \
	      && mv $(BUILD)/ops_full.csv $(BUILD)/ops.csv; \
	    cat $(BUILD)/instr.csv; \
	  else \
	    echo "  !! callgrind FAILED -- see $(BUILD)/instr.log"; \
	    tail -5 $(BUILD)/instr.log | sed 's/^/     /'; \
	    status=1; \
	  fi; \
	else \
	  echo; echo ">>> callgrind SKIPPED (valgrind not installed)"; \
	fi; \
	echo; \
	echo ">>> combined per-variant table"; \
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

# ============================================================================
# PWL coefficient tables.
#
# The tables are GENERATED and committed, so a plain build needs no Python.
# Regenerate only to change the segment width:
#
#   make pwl-tables P=4    regenerate at width 2^-4 (the committed default)
#   make pwl-sweep         accuracy vs table size, across segment widths
#
# P is the knee-finding knob: accuracy improves as 2^-2P until the Q14
# coefficients quantise, after which extra segments cost ROM and buy nothing.
# Re-tighten the suite tolerances after changing it -- see tests/.
# ============================================================================
P ?= 4

pwl-tables:
	@python3 scripts/trig_approx_parameters/gen_pwl_tables.py $(P) \
	    > src/common/trig_pwl_tables.h
	@echo "regenerated src/common/trig_pwl_tables.h at P=$(P)"
	@grep -m1 'summary' src/common/trig_pwl_tables.h || true

pwl-sweep:
	@./scripts/trig_approx_parameters/pwl_sweep.sh

clean:
	rm -rf $(BUILD) profiling/_profile_out
