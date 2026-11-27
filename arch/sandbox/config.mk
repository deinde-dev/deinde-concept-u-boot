# SPDX-License-Identifier: GPL-2.0+
# Copyright (c) 2011 The Chromium OS Authors.

PLATFORM_CPPFLAGS += -D__SANDBOX__ -U_FORTIFY_SOURCE
ifdef CONFIG_SANDBOX_WASM
PLATFORM_CPPFLAGS += -ffunction-sections -fdata-sections

# The textual assembler loses the type of WebAssembly globals, such as the
# stack pointer, so objects then fail to link
KBUILD_CFLAGS := $(filter-out -no-integrated-as,$(KBUILD_CFLAGS))
KBUILD_AFLAGS := $(filter-out -no-integrated-as,$(KBUILD_AFLAGS))

# emcc does not report where the compiler's own headers are in the usual way
NOSTDINC_FLAGS := -nostdinc -isystem $(shell $(CC) -print-resource-dir)/include
else
PLATFORM_CPPFLAGS += -fPIC -ffunction-sections -fdata-sections
endif

ifeq ($(CONFIG_BACKTRACE),y)
GCC_LIB_DIR := $(shell $(CC) -print-file-name=)
PLATFORM_LIBS += -L$(GCC_LIB_DIR) -lbacktrace
endif
PLATFORM_LIBS += -lrt -lpthread

ifdef GPROF
PLATFORM_LIBS += -pg
endif

SDL_CONFIG ?= sdl2-config

# Define this to avoid linking with SDL, which requires SDL libraries
# This can solve 'sdl-config: Command not found' errors
ifeq ($(CONFIG_SANDBOX_SDL),y)
PLATFORM_LIBS += $(shell $(SDL_CONFIG) --libs)
PLATFORM_CPPFLAGS += $(shell $(SDL_CONFIG) --cflags)
endif

SANITIZERS :=
ifdef CONFIG_ASAN
SANITIZERS	+= -fsanitize=address
# Anything that links libu-boot.a with --whole-archive (ulib tests, examples)
# pulls in AddressSanitizer-instrumented objects, so must also link libasan.
PLATFORM_LIBS	+= -lasan
endif
ifdef CONFIG_FUZZ
SANITIZERS	+= -fsanitize=fuzzer
endif
KBUILD_CFLAGS	+= $(SANITIZERS)

# Avoid defeating linker's garbage collection
ifeq ($(CONFIG_BACKTRACE)$(CONFIG_CMDLINE),yy)
RDYNAMIC += -rdynamic
endif

cmd_u-boot__ = \
	touch $(u-boot-main) ; \
	$(CC) -o $@ -Wl,-T u-boot.lds $(u-boot-init) \
	$(KBUILD_LDFLAGS:%=-Wl,%) \
	$(SANITIZERS) \
	$(LTO_FINAL_LDFLAGS) \
	-Wl,--whole-archive \
		$(u-boot-main) \
		$(u-boot-keep-syms-lto) \
	-Wl,--no-whole-archive \
	$(RDYNAMIC) $(PLATFORM_LIBS) -Wl,-Map -Wl,u-boot.map -Wl,--gc-sections

ifdef CONFIG_SANDBOX_WASM
# wasm-ld has no linker scripts, so cannot sort the linker lists. Instead,
# link U-Boot into a single relocatable object, sort the linker-list segments
# in that with a script, then do the final link. Asyncify lets os_usleep()
# return to the JavaScript event loop.
WASM_LDFLAGS := -sINITIAL_MEMORY=512MB -sSTACK_SIZE=4MB -sEXIT_RUNTIME=1 \
	-sMALLOC=emmalloc -sASYNCIFY -sASYNCIFY_STACK_SIZE=65536 -O2

# This produces a JavaScript loader called u-boot, along with u-boot.wasm,
# which runs under node and can see the host's files
cmd_u-boot__ = \
	touch $(u-boot-main) ; \
	$(LD) -r -o u-boot-unsorted.o $(u-boot-init) \
	--whole-archive \
		$(u-boot-main) \
	--no-whole-archive && \
	$(srctree)/scripts/wasm_sort_lists.py u-boot-unsorted.o \
		u-boot-sorted.o && \
	$(CC) -o $@ u-boot-sorted.o \
	$(WASM_LDFLAGS) -sNODERAWFS=1 -sENVIRONMENT=node \
	-Wl,-Map -Wl,u-boot.map

# This produces u-boot-web.js and u-boot-web.wasm for use in a web page. The
# devicetree is included, as /u-boot.dtb in the virtual filesystem.
quiet_cmd_u-boot-web = WASMLD  $@
cmd_u-boot-web = \
	$(CC) -o $@ u-boot-sorted.o \
	$(WASM_LDFLAGS) -sENVIRONMENT=web \
	--embed-file u-boot.dtb@/u-boot.dtb

u-boot-web.js: u-boot u-boot.dtb FORCE
	$(call if_changed,u-boot-web)

# objdump cannot read the loader, so take the symbols from the object which
# the loader is linked from. The Python tests use this to find unit tests.
cmd_sym = $(OBJDUMP) -t u-boot-sorted.o > $@
endif

cmd_u-boot-spl = (cd $(obj) && \
	touch $(patsubst $(obj)/%,%,$(u-boot-spl-main)) && \
	$(CC) -o $(XPL_BIN) -Wl,-T u-boot-spl.lds \
	$(KBUILD_LDFLAGS:%=-Wl,%) \
	$(SANITIZERS) \
	$(LTO_FINAL_LDFLAGS) \
	$(patsubst $(obj)/%,%,$(u-boot-spl-init)) \
	-Wl,--whole-archive \
		$(patsubst $(obj)/%,%,$(u-boot-spl-main)) \
		$(patsubst $(obj)/%,%,$(u-boot-spl-platdata)) \
		$(patsubst $(obj)/%,%,$(u-boot-spl-keep-syms-lto)) \
	-Wl,--no-whole-archive \
	$(PLATFORM_LIBS) -Wl,-Map -Wl,u-boot-spl.map -Wl,--gc-sections)

ifeq ($(HOST_ARCH),$(HOST_ARCH_X86_64))
EFI_LDS := ${SRCDIR}/../../../arch/x86/lib/elf_x86_64_efi.lds
EFI_TARGET := --output-target=efi-app-x86_64
else ifeq ($(HOST_ARCH),$(HOST_ARCH_X86))
EFI_LDS := ${SRCDIR}/../../../arch/x86/lib/elf_ia32_efi.lds
EFI_TARGET := --output-target=efi-app-ia32
else ifeq ($(HOST_ARCH),$(HOST_ARCH_AARCH64))
EFI_LDS := ${SRCDIR}/../../../arch/arm/lib/elf_aarch64_efi.lds
EFI_TARGET := --output-target=efi-app-aarch64
OBJCOPYFLAGS += -j .text -j .secure_text -j .secure_data -j .rodata -j .data \
		-j __u_boot_list -j .rela.dyn -j .got -j .got.plt \
		-j .binman_sym_table -j .text_rest \
		-j .efi_runtime -j .efi_runtime_rel
else ifeq ($(HOST_ARCH),$(HOST_ARCH_ARM))
EFI_LDS := ${SRCDIR}/../../../arch/arm/lib/elf_arm_efi.lds
OBJCOPYFLAGS += -j .text -j .secure_text -j .secure_data -j .rodata -j .hash \
		-j .data -j .got -j .got.plt -j __u_boot_list -j .rel.dyn \
		-j .binman_sym_table -j .text_rest \
		-j .efi_runtime -j .efi_runtime_rel
else ifeq ($(HOST_ARCH),$(HOST_ARCH_RISCV32))
EFI_LDS := ${SRCDIR}/../../../arch/riscv/lib/elf_riscv32_efi.lds
else ifeq ($(HOST_ARCH),$(HOST_ARCH_RISCV64))
EFI_LDS := ${SRCDIR}/../../../arch/riscv/lib/elf_riscv64_efi.lds
endif
EFI_CRT0 := crt0_sandbox_efi.o
EFI_RELOC := reloc_sandbox_efi.o

# U-Boot Library
LIB_LDS := $(srctree)/arch/sandbox/cpu/u-boot-lib.lds
LIB_STATIC_LDS := $(srctree)/arch/sandbox/cpu/ulib-test-static.lds
