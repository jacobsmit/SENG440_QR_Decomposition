# ============================================================================
# SENG 440 QR Decomposition -- variant x flagset build matrix
#
# See `make help`. Adding a variant: create src/variants/<name>/qr.c and add
# <name> to VARIANTS below.
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

# Measurement build only. At -O2 gcc turns the qr_profiled() wrappers into tail
# calls, and callgrind then never sees them return, so --toggle-collect leaves
# collection on and libc pours into the totals. Costs ~0.1%.
PROFILE_CFLAGS := -fno-optimize-sibling-calls

.PHONY: all test-all test-firmware check-tables hw-pkg hw-vectors hw-sim hw-gates profile-all static-all instr-all instr-detail cycles asip-asm compare clean help asm pwl-tables pwl-sweep
.PHONY: $(addprefix test-,$(VARIANTS)) $(addprefix profile-,$(VARIANTS))

all: test-all

help:
	@echo "software:  test-all compare profile-all instr-all instr-detail static-all asm"
	@echo "asip:      asip-asm givensq-asm test-firmware"
	@echo "hardware:  hw-sim hw-gates hw-pkg hw-vectors"
	@echo "tables:    pwl-tables P=n  pwl-sweep  check-tables"
	@echo "variants:  $(VARIANTS)"
	@echo "flagsets:  (see profiling/flagsets.sh)"

ITERATIONS ?= 1000
MAGNITUDE  ?= 8

# Per-variant rules, generated explicitly: GNU make excludes .PHONY targets
# from pattern-rule matching, so "test-%:" would silently do nothing.
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

# Dependencies rather than a shell loop, so a failing variant aborts make
# before the success banner can print.
# The firmware model is the specification givensq_fw.S must match bit-for-bit,
# so its own correctness is checked before any of the microcode claims rest on
# it: the restoring divider against exact division, and the coefficients
# against the two invariants that define a Givens rotation.
# The .S only builds on the ARM target; off-target the suite still checks the C
# model, and says out loud that the assembly comparison was skipped rather than
# quietly reporting success.
ifneq (,$(filter armv7l armv6l,$(UNAME_M)))
  FW_ASM := src/common/givensq_fw.S
else
  FW_ASM :=
endif

$(BUILD)/givensq_fw/test: tests/test_givensq_fw.c src/common/givensq_fw.c $(FW_ASM) $(COMMON_SRC) $(COMMON_HDR)
	@mkdir -p $(dir $@)
	$(CC) $(WARN) $(PORTABLE_CFLAGS) -o $@ \
	    tests/test_givensq_fw.c src/common/givensq_fw.c $(FW_ASM) $(COMMON_SRC) -lm

test-firmware: $(BUILD)/givensq_fw/test
	@$<

# The C, VHDL and ARM-assembly coefficient tables must be identical or the
# three implementations are not the same thing. They drifted once.
check-tables:
	@python3 scripts/check_tables_agree.py

test-all: check-tables test-firmware $(addprefix test-,$(VARIANTS))
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

# Exact dynamic instruction counts under callgrind. Slow (instrumentation on
# top of QEMU), but the per-QR figure is deterministic so a small sample is
# enough.
CG_ITERATIONS ?= 50

# Instantiate the custom instruction for real: emits a listing containing
# "GIVENSQ Rd, Rn, Rm", then shows the assembler rejecting it. Lesson 100 says
# that rejection is the expected outcome, so it is demonstrated, not hidden.
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
	@python3 scripts/gen_pwl_tables.py $(P) \
	    > src/common/trig_pwl_tables.h
	@echo "regenerated src/common/trig_pwl_tables.h at P=$(P)"
	@grep -m1 'summary' src/common/trig_pwl_tables.h || true

pwl-sweep:
	@./scripts/pwl_sweep.sh

# Regenerate the firmware assembly from the CSD table. Generated, not hand
# typed, so the 45 segment routines cannot drift from the C model's tables.
givensq-asm:
	@python3 scripts/gen_givensq_asm.py \
	    > src/common/givensq_fw.S
	@echo "regenerated src/common/givensq_fw.S"
	@if [ -n "$(ARM_CC)" ]; then \
	  $(ARM_CC) -c -marm -march=armv7-a -o $(BUILD)/givensq_fw.o \
	      src/common/givensq_fw.S && \
	  echo "  assembles clean; mul/div instructions: $$($(ARM_DUMP) -d \
	      $(BUILD)/givensq_fw.o | grep -ciE '\\b(mul|mla|smull|umull|sdiv|udiv)\\b')"; \
	fi

# ============================================================================
# Hardware (scenario step 8): VHDL, testbench, simulated latency, gate count.
#
#   make hw-sim      analyse + elaborate + run the testbench (needs ghdl)
#   make hw-gates    gate-equivalent estimate with the arithmetic shown
#   make hw-pkg      regenerate the coefficient ROMs
#   make hw-vectors  regenerate the test vectors from the C model
#
# The ROMs and the vectors both come from the C firmware model, so the C, the
# ARM assembly and the VHDL cannot drift apart -- one vector file checks all
# three.
# ============================================================================
GHDL ?= ghdl
GHDL_FLAGS := --std=08 --workdir=$(BUILD)/ghdl

hw-pkg:
	@python3 scripts/gen_csd_header.py > src/common/trig_pwl_csd.h
	@python3 scripts/gen_vhdl_pkg.py > hw/givensq_pkg.vhd
	@python3 scripts/gen_givensq_asm.py > src/common/givensq_fw.S
	@echo "regenerated the C header, the VHDL package and the ARM assembly"
	@python3 scripts/check_tables_agree.py

hw-vectors: hw/vectors.txt

hw/vectors.txt: tools/gen_hw_vectors.c src/common/givensq_fw.c $(COMMON_HDR)
	@mkdir -p $(BUILD)
	@$(CC) -O2 -o $(BUILD)/gen_hw_vectors tools/gen_hw_vectors.c src/common/givensq_fw.c
	@./$(BUILD)/gen_hw_vectors hw/vectors.txt

hw-sim: hw/vectors.txt
	@if ! command -v $(GHDL) >/dev/null 2>&1; then \
	  echo "ERROR: ghdl not installed (apt-get install ghdl)"; exit 1; fi
	@mkdir -p $(BUILD)/ghdl
	@$(GHDL) -a $(GHDL_FLAGS) hw/givensq_pkg.vhd
	@$(GHDL) -a $(GHDL_FLAGS) hw/givensq.vhd
	@$(GHDL) -a $(GHDL_FLAGS) hw/tb_givensq.vhd
	@$(GHDL) -e $(GHDL_FLAGS) tb_givensq
	@$(GHDL) -r $(GHDL_FLAGS) tb_givensq --assert-level=error

hw-gates:
	@python3 scripts/gate_count.py

clean:
	rm -rf $(BUILD) profiling/_profile_out
